/**
 * @file uart_comm.h
 * @brief UART 串口通信接口
 *
 * 封装 Linux 下 termios 串口配置，提供打开/关闭/收发接口。
 * 固定配置：8N1（8 数据位、1 停止位、无校验）、无硬件流控。
 * 默认波特率 115200（cfsetispeed/cfsetospeed 硬编码），
 * uart_open 的 baudrate 参数为后续扩展预留。
 *
 * 读模式：VMIN=0, VTIME=1（阻塞最多 10ms 返回），配合主循环 usleep(100ms)。
 * 写模式：循环 write 确保完整发送，处理 EINTR 和 short write。
 */
#ifndef __UART_COMM_H
#define __UART_COMM_H
#include <stdint.h>

/**
 * @brief 打开串口设备并配置 8N1/无流控
 * @param dev      设备节点路径，如 "/dev/ttyS3"
 * @param baudrate 波特率（当前忽略，硬编码 115200）
 * @return 成功返回 fd（>=0），失败返回 -1
 */
int uart_open(const char *dev, int baudrate);

/**
 * @brief 发送数据（循环写保证完整）
 * @param buf 发送缓冲区
 * @param len 待发送字节数
 * @return 实际发送字节数，失败返回 -1
 */
int uart_send_data(uint8_t *buf, int len);

/**
 * @brief 接收数据（最多等 10ms）
 * @param buf    接收缓冲区
 * @param max_len 缓冲区大小
 * @return 实际读取字节数，0 表示无数据，失败返回 -1
 */
int uart_recv_data(uint8_t *buf, int max_len);

/**
 * @brief 关闭串口，重置内部 fd
 */
void uart_close(void);

#endif
