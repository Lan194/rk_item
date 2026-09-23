/**
 * @file hw_touch.c
 * @brief TSC2046 电阻屏驱动 + 自动校准坐标映射
 *
 * 核心设计：
 *   不同于常规用 EVIOCGABS 查驱动声明范围做映射，
 *   这里改为**运行时自动校准**——程序启动时把范围设为极端值，
 *   用户每摸一个点就扩展 min/max，摸过几个角后自然收敛到屏幕物理范围。
 *
 *   为什么不用 EVIOCGABS？
 *   TSC2046 驱动声明 ABS_X/ABS_Y 范围 0~4095（12-bit ADC 全量程），
 *   但屏幕实际可触摸区域只覆盖其中一部分（如 raw X=800~3700），
 *   用 0~4095 映射会导致：原点偏移、满屏顶不到 2048。
 *
 *   自动校准流程：
 *     初始  abs_x_min=4095, abs_x_max=0   (极端值，等待学习)
 *     触摸时 abs_x_min = min(abs_x_min, raw_x)
 *            abs_x_max = max(abs_x_max, raw_x)
 *     当 abs_x_max - abs_x_min > 500 且  abs_y_max - abs_y_min > 500
 *     → 校准完成，打印 CALIBRATED 日志
 *
 *   坐标映射：
 *     X 反转: out = 2048 - ((raw - min) * 2048 / (max - min))
 *     Y 直通: out =        ((raw - min) * 2048 / (max - min))
 *     反转原因：TSC2046 默认左=大右=小，协议要求左=0右=2048
 */
#include "hw_touch.h"
#include "log_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/ioctl.h>

static int touch_fd = -1;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static uint8_t  last_pressed = 0;

/*协议要求坐标范围 0~2048*/
#define TOUCH_OUT_MAX 2048

/*ADC 理论全量程（12-bit），仅用于初始极端值和钳位上限*/
#define RAW_ADC_MAX 4095

/*自动校准：初始 min 设最大、max 设最小，等待触摸学习后自然收敛*/
static int abs_x_min = RAW_ADC_MAX, abs_x_max = 0;
static int abs_y_min = RAW_ADC_MAX, abs_y_max = 0;

/*校准完成判定：触摸覆盖范围超过 500 个 ADC 单位即认为已学习到足够区域*/
#define CALIB_RANGE_THRESH 500
static int calibrated = 0;

/*节流日志：每 20 次 ABS 事件打一条，避免刷屏阻塞主循环*/
static int log_throttle = 0;

/*把原始 ADC 值映射到 0~2048，并可选反转方向*/
static uint16_t map_coord(int raw, int raw_min, int raw_max, int invert)
{
    int raw_range = raw_max - raw_min;
    if(raw_range <= 0) return 0;

    int v = raw - raw_min;
    if(v < 0) v = 0;
    if(v > raw_range) v = raw_range;

    uint16_t out = (uint16_t)((v * TOUCH_OUT_MAX) / raw_range);
    if(invert) out = TOUCH_OUT_MAX - out;
    return out;
}

/*触摸时动态更新校准范围*/
static void calib_update(int raw_x, int raw_y)
{
    int old_x_range = abs_x_max - abs_x_min;
    int old_y_range = abs_y_max - abs_y_min;

    if(raw_x < abs_x_min) abs_x_min = raw_x;
    if(raw_x > abs_x_max) abs_x_max = raw_x;
    if(raw_y < abs_y_min) abs_y_min = raw_y;
    if(raw_y > abs_y_max) abs_y_max = raw_y;

    /*范围显著扩大时打印，方便看到学习过程*/
    int new_x_range = abs_x_max - abs_x_min;
    int new_y_range = abs_y_max - abs_y_min;
    if(new_x_range > old_x_range + 100 || new_y_range > old_y_range + 100)
    {
        LOG_I("touch calib X[%d..%d] Y[%d..%d]\n",
              abs_x_min, abs_x_max, abs_y_min, abs_y_max);
    }

    /*校准完成判定*/
    if(!calibrated &&
       (abs_x_max - abs_x_min) > CALIB_RANGE_THRESH &&
       (abs_y_max - abs_y_min) > CALIB_RANGE_THRESH)
    {
        calibrated = 1;
        LOG_I("touch CALIBRATED  X[%d..%d] Y[%d..%d]\n",
              abs_x_min, abs_x_max, abs_y_min, abs_y_max);
    }
}

static int touch_probe(void)
{
    DIR *d = opendir("/dev/input");
    if(!d) { LOG_E("/dev/input not found\n"); return -1; }
    int found = -1;
    struct dirent *e;
    while((e = readdir(d)) != NULL)
    {
        if(strncmp(e->d_name, "event", 5) != 0) continue;
        char path[512];
        snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);

        int fd = open(path, O_RDWR | O_NONBLOCK);
        if(fd < 0) continue;

        unsigned long evbits[EV_MAX/8 + 1] = {0};
        if(ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits) < 0)
        {
            close(fd); continue;
        }
        if(evbits[EV_ABS/8] & (1 << (EV_ABS % 8)))
        {
            LOG_I("touch dev=%s (auto-calibration mode)\n", path);
            found = fd;
            break;
        }
        close(fd);
    }
    closedir(d);
    return found;
}

int hw_touch_init(void)
{
    int fd = touch_probe();
    if(fd < 0)
    {
        LOG_I("touch device not found, touch feature disabled\n");
        return -1;
    }
    touch_fd = fd;
    LOG_I("touch init: please touch all 4 corners to calibrate\n");
    return 0;
}

int hw_touch_poll(uint16_t *x, uint16_t *y, uint8_t *pressed)
{
    if(touch_fd < 0) return -2;

    struct input_event ev;
    int got = 0;
    int prev_pressed = last_pressed;
    int raw_x = -1, raw_y = -1;

    while(1)
    {
        ssize_t n = read(touch_fd, &ev, sizeof(ev));
        if(n < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG_E("touch read err: %s\n", strerror(errno));
            return -2;
        }
        if(n != sizeof(ev)) continue;

        switch(ev.type)
        {
        case EV_ABS:
            if(ev.code == ABS_X)
            {
                raw_x = ev.value;
                /*X 轴反转：TSC2046 默认左大右小，协议要求左=0右=2048*/
                last_x = map_coord(raw_x, abs_x_min, abs_x_max, 1);
                got = 1;
            }
            else if(ev.code == ABS_Y)
            {
                raw_y = ev.value;
                /*Y 轴直通：上=0 下=2048*/
                last_y = map_coord(raw_y, abs_y_min, abs_y_max, 0);
                got = 1;
            }
            else if(ev.code == ABS_PRESSURE)
            {
                last_pressed = ev.value > 0 ? 1 : 0;
                got = 1;
            }
            break;
        case EV_KEY:
            if(ev.code == BTN_TOUCH)
            {
                last_pressed = ev.value ? 1 : 0;
                got = 1;
            }
            break;
        case EV_SYN:
            break;
        default:
            break;
        }
    }

    /*触摸时更新自动校准范围*/
    if(raw_x >= 0 && raw_y >= 0)
    {
        calib_update(raw_x, raw_y);
        log_throttle++;
        if(log_throttle >= 20)
        {
            log_throttle = 0;
            LOG_I("touch raw=(%d,%d) mapped=(%u,%u) %s\n",
                  raw_x, raw_y, last_x, last_y,
                  calibrated ? "[calibrated]" : "[learning...]");
        }
    }

    /*触摸抬起瞬间坐标归零，避免上位机显示"卡住"*/
    if(prev_pressed && !last_pressed)
    {
        last_x = 0;
        last_y = 0;
    }

    if(x) *x = last_x;
    if(y) *y = last_y;
    if(pressed) *pressed = last_pressed;
    return got ? 0 : -1;
}

void hw_touch_close(void)
{
    if(touch_fd >= 0)
    {
        close(touch_fd);
        touch_fd = -1;
    }
}
