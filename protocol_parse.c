#include "protocol_parse.h"
#include "uart_comm.h"
#include "business.h"
#include "hw_driver.h"
#include "log_util.h"

static uint8_t calc_crc8_xor(uint8_t *data,int len)
{
    uint8_t crc=0;
    for(int i=0;i<len;i++)
    {
        crc ^= data[i];
    }
    return crc;
}

/*应答帧，普通指令8字节应答*/
static void send_ptu_ack(uint8_t func,uint8_t result)
{
    uint8_t ack[8]={0xAA,0x55,func,result,0,0,0,0};
    uint8_t crc = calc_crc8_xor(ack,7);
    ack[7]=crc;
    uart_send_data(ack,8);
}

/*从环形buf取一字节*/
static int ring_get_byte(uint8_t *ring,uint16_t sz,uint16_t *rd,uint16_t *wr,uint8_t *out)
{
    if(*rd == *wr) return -1;
    *out = ring[*rd];
    *rd = (*rd + 1) % sz;
    return 0;
}

/*环形缓冲区读指针回退n个字节*/
static void ring_rollback(uint16_t ring_size, uint16_t *p_rd, uint16_t n)
{
    *p_rd = (uint16_t)((*p_rd + ring_size - n) % ring_size);
}

void protocol_parse_ptu_cmd(uint8_t *ring, uint16_t ring_size, uint16_t *p_rd, uint16_t *p_wr)
{
    uint8_t b;
    while(0 == ring_get_byte(ring, ring_size, p_rd, p_wr, &b))
    {
        if(b != 0x55) continue;

        /*0x55已被取走，rd-1即帧头位置；帧不完整时整体回退到这里*/
        uint16_t frame_start = (uint16_t)((*p_rd + ring_size - 1) % ring_size);

        /*找第二个帧头字节0xAA*/
        if(0 != ring_get_byte(ring, ring_size, p_rd, p_wr, &b))
        {
            *p_rd = frame_start; /*半包：只收到0x55*/
            return;
        }
        if(b != 0xAA)
        {
            /*当前字节可能本身就是0x55(连续帧头)，回退一字节重新扫描*/
            ring_rollback(ring_size, p_rd, 1);
            continue;
        }

        /*已经命中帧头 0x55 0xAA，取功能码*/
        uint8_t func;
        if(0 != ring_get_byte(ring, ring_size, p_rd, p_wr, &func))
        {
            *p_rd = frame_start; /*半包：帧头后无功能码*/
            return;
        }

        int frame_len = (func == 0x04) ? 11 : 8; /*校时11字节，其余8字节*/

        uint8_t frame_buf[11]={0};
        frame_buf[0]=0x55;
        frame_buf[1]=0xAA;
        frame_buf[2]=func;

        /*读取剩下的数据字节*/
        for(int i=3;i<frame_len;i++)
        {
            if(0 != ring_get_byte(ring, ring_size, p_rd, p_wr, &frame_buf[i]))
            {
                /*半包：回退到帧头，已收字节全部保留，等待下一轮解析*/
                *p_rd = frame_start;
                return;
            }
        }

        /*CRC校验，失败帧直接丢弃（读指针已越过该帧），继续向后扫描*/
        uint8_t crc_calc = calc_crc8_xor(frame_buf, frame_len - 1);
        if(crc_calc != frame_buf[frame_len - 1])
        {
            LOG_E("frame func=0x%02X CRC8 err recv=0x%02X calc=0x%02X\n",
                  func, frame_buf[frame_len - 1], crc_calc);
            continue;
        }

        /*执行指令*/
        switch(func)
        {
        case 0x01:/*背光开关 frame_buf[3]有效*/
            g_dev_state.backlight_en = frame_buf[3]?1:0;
            hw_backlight_switch(g_dev_state.backlight_en);
            send_ptu_ack(0x01,1);
            LOG_I("CMD backlight_switch:%d\n",frame_buf[3]);
            break;
        case 0x02:/*背光档位1‑5*/
            if(frame_buf[3]>=1 && frame_buf[3]<=5)
            {
                g_dev_state.backlight_level = frame_buf[3];
                hw_backlight_set_level(frame_buf[3]);
                send_ptu_ack(0x02,1);
                LOG_I("CMD backlight_level:%d\n",frame_buf[3]);
            }
            else
            {
                send_ptu_ack(0x02,0);
                LOG_E("backlight level invalid\n");
            }
            break;
        case 0x03:/*蜂鸣器开关*/
            g_dev_state.beep_en = frame_buf[3]?1:0;
            hw_beep_set(g_dev_state.beep_en);
            send_ptu_ack(0x03,1);
            LOG_I("CMD beep:%d\n",frame_buf[3]);
            break;
        case 0x04:/*校时指令11字节：[3..4]年大端，[5]月[6]日[7]时[8]分[9]秒*/
        {
            uint16_t year = ((uint16_t)frame_buf[3] << 8) | frame_buf[4];
            int ok = business_sync_time(year, frame_buf[5], frame_buf[6],
                                        frame_buf[7], frame_buf[8], frame_buf[9]);
            send_ptu_ack(0x04, ok == 0 ? 1 : 0);
            LOG_I("CMD time-sync %04u-%02u-%02u %02u:%02u:%02u %s\n",
                  year, frame_buf[5], frame_buf[6], frame_buf[7],
                  frame_buf[8], frame_buf[9], ok == 0 ? "ok" : "set-sys-fail");
            break;
        }
        default:
            send_ptu_ack(func,0);
            LOG_E("unknown func 0x%02X\n",func);
            break;
        }
    }
}
