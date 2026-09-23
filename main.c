#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "uart_comm.h"
#include "protocol_pack.h"
#include "protocol_parse.h"
#include "business.h"
#include "hw_driver.h"
#include "log_util.h"

#define RING_BUF_SIZE 512
uint8_t ring_buf[RING_BUF_SIZE];
uint16_t ring_wr = 0;
uint16_t ring_rd = 0;

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
    LOG_I("RK3506 comm software start\n");
    business_init();
    hw_driver_init();

    if(uart_open("/dev/ttyS3",115200) < 0)
    {
        LOG_E("uart open /dev/ttyS3 fail\n");
        return -1;
    }
    LOG_I("uart /dev/ttyS3 open success\n");

    uint32_t tick_extend = 0;
    uint32_t tick_env = 0;
    uint8_t temp_read[128];
    int ret;

    while(1)
    {
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
            business_update_time(); /*每秒同步系统时钟到报文时间字段*/
            uint8_t frame[64];
            int len = pack_sensor_status_frame(frame, &g_dev_state);
            if(len > 0)
            {
                uart_send_data(frame, len);
            }
            tick_env = 0;
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
    }

    uart_close();
    return 0;
}
