#ifndef __PROTOCOL_PACK_H
#define __PROTOCOL_PACK_H
#include <stdint.h>
#include "business.h"

/*扩展坞状态报文：帧头0xAA 0xBB id0x01，总长度48字节(0x30)*/
int pack_dock_status_frame(uint8_t *out, business_state_t *s);
/*环境感知模块状态报文：帧头0xEE 0xFF id0x21，总长度48字节(0x30)*/
int pack_sensor_status_frame(uint8_t *out, business_state_t *s);
#endif
