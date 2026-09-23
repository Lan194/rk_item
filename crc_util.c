#include "crc_util.h"
/*CRC16‑MODBUS，初始0xFFFF，多项式0xA001，文档板上报报文使用*/
uint16_t crc16_calc(uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    for(int i=0;i<len;i++)
    {
        crc ^= data[i];
        for(int j=0;j<8;j++)
        {
            if(crc & 0x0001)
                crc = (crc>>1) ^ 0xA001;
            else
                crc >>=1;
        }
    }
    return crc;
}

/*PTU下行指令CRC8：多项式0x07，初值0x00，MSB优先，无反射，无输出异或*/
uint8_t crc8_calc(uint8_t *data, int len)
{
    uint8_t crc = 0x00;
    for(int i=0;i<len;i++)
    {
        crc ^= data[i];
        for(int j=0;j<8;j++)
        {
            if(crc & 0x80)
                crc = (uint8_t)((crc<<1) ^ 0x07);
            else
                crc = (uint8_t)(crc<<1);
        }
    }
    return crc;
}
