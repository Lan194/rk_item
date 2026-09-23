#include "protocol_pack.h"
#include "crc_util.h"
#include <string.h>

static void put_u16_be(uint8_t *p, uint16_t v)
{
    p[0] = (v>>8)&0xFF;
    p[1] = v&0xFF;
}
static void put_u32_be(uint8_t *p, uint32_t v)
{
    p[0]=(v>>24)&0xFF;
    p[1]=(v>>16)&0xFF;
    p[2]=(v>>8)&0xFF;
    p[3]=v&0xFF;
}

/*扩展坞状态报文，总48字节，0x30长度，CRC计算0‑45字节，输出46、47大端CRC16*/
int pack_dock_status_frame(uint8_t *out, business_state_t *s)
{
    memset(out,0,48);
    out[0]=0xAA; out[1]=0xBB;
    out[2]=0x01;
    out[3]=0x30;
    put_u32_be(&out[4], s->seq++);

    put_u16_be(&out[8], s->year);
    out[10] = s->mon;
    out[11] = s->day;
    out[12] = s->hour;
    out[13] = s->min;
    out[14] = s->sec;

    out[15] = s->mem_usage;
    out[16] = s->cpu_usage;
    out[17] = s->disk_usage;
    out[18] = 2; /*心跳间隔2秒文档规定*/

    /*19‑45全部填0预留*/
    uint16_t crc = crc16_calc(out,46); /*0~45共46字节*/
    put_u16_be(&out[46], crc);
    return 48;
}

/*环境感知模块状态报文，总48字节，0x30长度*/
int pack_sensor_status_frame(uint8_t *out, business_state_t *s)
{
    memset(out,0,48);
    out[0]=0xEE; out[1]=0xFF;
    out[2]=0x21;
    out[3]=0x30;
    put_u32_be(&out[4], s->seq++);

    put_u16_be(&out[8],  (uint16_t)(s->temp*10));
    put_u16_be(&out[10], s->hum);
    put_u16_be(&out[12], s->press);
    put_u16_be(&out[14], s->alt);
    put_u16_be(&out[16], s->o2);
    put_u16_be(&out[18], s->co);
    put_u16_be(&out[20], s->h2s);
    put_u16_be(&out[22], s->ch4);

    /*罗盘、X/Y坐标、预留全部置0*/
    uint16_t crc = crc16_calc(out,46);
    put_u16_be(&out[46], crc);
    return 48;
}
