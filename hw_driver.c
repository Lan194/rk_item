#include "hw_driver.h"
#include "log_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#define BEEP_GPIO 24
#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_VAL_PATH "/sys/class/gpio/gpio24/value"

/*RK3506 DTS中pwm-backlight节点会注册成backlight class设备，
  内部由PWM驱动占空比，用户态通过/sys/class/backlight控制即可*/
#define BACKLIGHT_CLASS_DIR "/sys/class/backlight"
#define BL_POWER_ON   0  /*FB_BLANK_UNBLANK*/
#define BL_POWER_OFF  4  /*FB_BLANK_POWERDOWN*/

/*d_name最长256，路径缓冲按理论最大值预留*/
static char bl_dir[280] = {0};
static int  bl_max_brightness = 255;

static int sysfs_write_str(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if(fd < 0) return -1;
    int ret = (int)write(fd, val, strlen(val));
    close(fd);
    return ret;
}

/*探测backlight class设备，取第一个；读取max_brightness*/
static int backlight_probe(void)
{
    DIR *d = opendir(BACKLIGHT_CLASS_DIR);
    if(d == NULL)
    {
        LOG_E("backlight class dir not found\n");
        return -1;
    }
    struct dirent *e;
    while((e = readdir(d)) != NULL)
    {
        if(e->d_name[0] == '.') continue;
        snprintf(bl_dir, sizeof(bl_dir), "%s/%s", BACKLIGHT_CLASS_DIR, e->d_name);
        break;
    }
    closedir(d);

    if(bl_dir[0] == 0)
    {
        LOG_E("no backlight device\n");
        return -1;
    }

    char path[300], buf[32] = {0};
    snprintf(path, sizeof(path), "%s/max_brightness", bl_dir);
    int fd = open(path, O_RDONLY);
    if(fd >= 0)
    {
        if(read(fd, buf, sizeof(buf)-1) > 0)
        {
            int v = atoi(buf);
            if(v > 0) bl_max_brightness = v;
        }
        close(fd);
    }
    LOG_I("backlight dev=%s max_brightness=%d\n", bl_dir, bl_max_brightness);
    return 0;
}

static int gpio_export(int num)
{
    int fd = open(GPIO_EXPORT,O_WRONLY);
    if(fd<0) return -1;
    char tmp[16];
    sprintf(tmp,"%d",num);
    write(fd,tmp,strlen(tmp));
    close(fd);
    return 0;
}

static int gpio_set_output(int num)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", num);
    int fd = open(path,O_WRONLY);
    if(fd<0) return -1;
    write(fd,"out",3);
    close(fd);
    return 0;
}

int hw_driver_init(void)
{
    LOG_I("hw driver init\n");
    gpio_export(BEEP_GPIO);
    gpio_set_output(BEEP_GPIO);
    hw_beep_set(0);
    backlight_probe(); /*探测失败不影响蜂鸣器等其他功能*/
    return 0;
}

int hw_backlight_switch(int en)
{
    if(bl_dir[0] == 0)
    {
        LOG_E("backlight switch fail: device not found\n");
        return -1;
    }
    char path[300];
    snprintf(path, sizeof(path), "%s/bl_power", bl_dir);
    if(sysfs_write_str(path, en ? "0" : "4") < 0)
    {
        LOG_E("backlight switch write fail\n");
        return -1;
    }
    LOG_I("backlight switch en=%d\n",en);
    return 0;
}

int hw_backlight_set_level(int level)
{
    if(level < 1 || level > 5)
    {
        LOG_E("backlight level invalid:%d\n", level);
        return -1;
    }
    if(bl_dir[0] == 0)
    {
        LOG_E("backlight set level fail: device not found\n");
        return -1;
    }

    /*1‑5档线性映射到 max_brightness 的 20%~100%*/
    int bright = bl_max_brightness * level / 5;
    if(bright < 1) bright = 1;

    char path[300], val[16];
    snprintf(path, sizeof(path), "%s/brightness", bl_dir);
    snprintf(val, sizeof(val), "%d", bright);
    if(sysfs_write_str(path, val) < 0)
    {
        LOG_E("backlight brightness write fail\n");
        return -1;
    }

    /*调档时确保背光处于点亮状态*/
    snprintf(path, sizeof(path), "%s/bl_power", bl_dir);
    sysfs_write_str(path, "0");

    LOG_I("backlight level=%d brightness=%d\n", level, bright);
    return 0;
}

int hw_beep_set(int en)
{
    int fd = open(GPIO_VAL_PATH,O_WRONLY);
    if(fd<0)
    {
        LOG_E("beep gpio open fail\n");
        return -1;
    }
    if(en)
        write(fd,"1",1);
    else
        write(fd,"0",1);
    close(fd);
    LOG_I("beep %s\n",en?"ON":"OFF");
    return 0;
}
