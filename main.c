#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include "uart_comm.h"
#include "protocol_pack.h"
#include "protocol_parse.h"
#include "business.h"
#include "hw_driver.h"
#include "hw_touch.h"
#include "log_util.h"

#define RING_BUF_SIZE 512
uint8_t ring_buf[RING_BUF_SIZE];
uint16_t ring_wr = 0;
uint16_t ring_rd = 0;

static volatile int g_running = 1;
static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void ring_push(uint8_t *src, int len)
{
    for(int i=0;i<len;i++)
    {
        uint16_t next = (ring_wr + 1) % RING_BUF_SIZE;
        if(next == ring_rd) break;
        ring_buf[ring_wr] = src[i];
        ring_wr = next;
    }
}

int main(void)
{
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    LOG_I("RK3506 comm software start\n");
    business_init();
    hw_driver_init();

    if(uart_open("/dev/ttyS3",115200) < 0)
    {
        LOG_E("uart open /dev/ttyS3 fail\n");
        return -1;
    }
    LOG_I("uart /dev/ttyS3 open success\n");

    hw_touch_init(); /*触摸设备自动探测，失败不影响主流程*/

    uint32_t tick_extend = 0;
    uint32_t tick_env = 0;
    uint32_t touch_throttle = 0; /*触摸节流：100ms 内不重复发送 sensor 帧*/
    uint8_t temp_read[128];
    int ret;

    while(g_running)
    {
        /*时间刷新——每轮都调，time() 开销极小，不跟任何 tick 耦合*/
        business_update_time();

        /*系统监控 CPU/内存/磁盘 500ms 周期，跟 dock 帧对齐*/
        if(tick_extend >= 500)
        {
            business_update_sysinfo();
        }

        /*扩展坞状态报文 500ms周期，帧头0xAA 0xBB id0x01，48字节*/
        if(tick_extend >= 500)
        {
            uint8_t frame[64];
            int len = pack_dock_status_frame(frame, &g_dev_state);
            if(len > 0)
            {
                uart_send_data(frame, len);
            }
            tick_extend = 0;
        }

        /*环境感知状态报文 1000ms周期，帧头0xEE 0xFF id0x21，48字节*/
        if(tick_env >= 1000)
        {
            uint8_t frame[64];
            int len = pack_sensor_status_frame(frame, &g_dev_state);
            if(len > 0)
            {
                uart_send_data(frame, len);
            }
            tick_env = 0;
        }

        /*触摸坐标读取（非阻塞，100ms 周期）*/
        int touch_ret = hw_touch_poll(&g_dev_state.touch_x, &g_dev_state.touch_y, &g_dev_state.touch_pressed);
        if(touch_ret == 0 && touch_throttle >= 100)
        {
            /*触摸有新事件——节流后发送 sensor 帧*/
            uint8_t frame[64];
            int len = pack_sensor_status_frame(frame, &g_dev_state);
            if(len > 0) uart_send_data(frame, len);
            touch_throttle = 0;
        }

        /*读取字节送入环形缓冲区，解决粘包半包*/
        ret = uart_recv_data(temp_read, sizeof(temp_read));
        if(ret > 0)
        {
            ring_push(temp_read, ret);
            /*从ringbuf解析完整帧*/
            protocol_parse_ptu_cmd(ring_buf, RING_BUF_SIZE, &ring_rd, &ring_wr);
        }

        usleep(100000);
        tick_extend += 100;
        tick_env += 100;
        touch_throttle += 100;
    }

    LOG_I("exit, cleaning up...\n");
    hw_touch_close();
    uart_close();
    return 0;
}
