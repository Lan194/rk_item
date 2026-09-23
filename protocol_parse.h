/**
 * @file protocol_parse.h
 * @brief PTU 下行指令帧解析接口
 *
 * 帧格式（8 字节）：
 *   字节 0   帧头     0x55
 *   字节 1   帧头     0xAA
 *   字节 2   功能码   0x01~0x04
 *   字节 3~6 数据     4 字节（各功能码定义见 switch 分支）
 *   字节 7   CRC8     覆盖 0~6 字节
 *
 * 0x04 校时指令为 11 字节特例：数据区 8 字节（年大端 2 + 月日时分秒各 1），
 * CRC 在字节 10，覆盖 0~9 字节。
 */
#ifndef __PROTOCOL_PARSE_H
#define __PROTOCOL_PARSE_H
#include <stdint.h>

/**
 * @brief 从环形缓冲区中解析 PTU 下行指令
 * @param ring      环形缓冲区数组
 * @param ring_size 缓冲区大小（字节）
 * @param p_rd      [in/out] 读指针；解析过程中前移；半包时回退
 * @param p_wr      [in]     写指针
 *
 * 内部包含半包活锁保护：
 *   当环形缓冲区尾部只有半个帧头时，parser 会回退 rd 等待下一轮。
 *   若连续 5 次（约 500ms）仍无法凑齐一帧，则跳过那个残留帧头重新扫描。
 */
void protocol_parse_ptu_cmd(uint8_t *ring, uint16_t ring_size,
                            uint16_t *p_rd, uint16_t *p_wr);

#endif
