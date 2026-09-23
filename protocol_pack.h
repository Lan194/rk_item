/**
 * @file protocol_pack.h
 * @brief 板端上报状态帧打包接口
 *
 * 两类报文（均 48 字节，大端编码，CRC16-Modus 覆盖 0~45 字节）：
 *
 *   扩展坞状态报文 (dock)  —— 帧头 0xAA 0xBB, id 0x01, 500ms 周期
 *     out[0]    = 0xAA
 *     out[1]    = 0xBB
 *     out[2]    = 0x01
 *     out[3]    = 0x30 (48 字节)
 *     out[4..7] = seq (4 字节大端)
 *     out[8..9] = year
 *     out[10]   = mon
 *     out[11]   = day
 *     out[12]   = hour
 *     out[13]   = min
 *     out[14]   = sec
 *     out[15]   = mem_usage
 *     out[16]   = cpu_usage
 *     out[17]   = disk_usage
 *     out[18]   = 2 (心跳间隔 2 秒)
 *     out[19..45] = 0 (预留)
 *     out[46..47] = CRC16 高/低字节
 *
 *   环境感知状态报文 (sensor) —— 帧头 0xEE 0xFF, id 0x21, 1000ms 周期 + 触摸即时发送
 *     out[0..7]  同上（帧头+id+len+seq）
 *     out[8..9]  = temp*10
 *     out[10..11]= hum
 *     out[12..13]= press
 *     out[14..15]= alt
 *     out[16..17]= o2
 *     out[18..19]= co
 *     out[20..21]= h2s
 *     out[22..23]= ch4
 *     out[24..25]= compass     (罗盘航向角)
 *     out[26..27]= touch_x     (触摸 X)
 *     out[28..29]= touch_y     (触摸 Y)
 *     out[30..45]= 0 (预留)
 *     out[46..47]= CRC16
 */
#ifndef __PROTOCOL_PACK_H
#define __PROTOCOL_PACK_H
#include <stdint.h>
#include "business.h"

/**
 * @brief 打包扩展坞状态报文
 * @param out 输出缓冲区（至少 48 字节）
 * @param s   业务状态指针
 * @return 固定 48
 */
int pack_dock_status_frame(uint8_t *out, business_state_t *s);

/**
 * @brief 打包环境感知状态报文
 * @param out 输出缓冲区（至少 48 字节）
 * @param s   业务状态指针
 * @return 固定 48
 */
int pack_sensor_status_frame(uint8_t *out, business_state_t *s);

#endif
