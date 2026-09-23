/**
 * @file crc_util.h
 * @brief CRC 校验算法集合
 *
 * 本项目涉及两种 CRC：
 *   1. CRC8  —— PTU 上位机下发指令的帧尾校验
 *   2. CRC16 —— 板端上报状态报文的帧尾校验（Modbus 变体）
 *
 * 多项式来源说明：
 *   crc8_calc  是通过 6 条抓取的下行指令反推得出，
 *              6 组 (帧数据, CRC) 全部命中 poly=0x07 init=0x00,
 *              故可确定为 PTU 协议要求。
 *   crc16_calc 按协议文档实现 Modbus 算法（poly=0xA001 init=0xFFFF）。
 */
#ifndef __CRC_UTIL_H
#define __CRC_UTIL_H
#include <stdint.h>

/**
 * @brief CRC16-Modbus 校验
 *        初始值 0xFFFF，多项式 0xA001（反转 0x8005），LSB 优先
 * @param data 待校验数据
 * @param len  数据长度
 * @return 16 位校验值（协议中高字节在前发送）
 */
uint16_t crc16_calc(uint8_t *data, int len);

/**
 * @brief PTU 下行指令 CRC8 校验
 *        多项式 0x07，初值 0x00，MSB 优先，无反射，无异或输出
 * @param data 待校验数据（不含 CRC 字节本身）
 * @param len  数据长度
 * @return 8 位校验值
 */
uint8_t  crc8_calc(uint8_t *data, int len);

#endif
