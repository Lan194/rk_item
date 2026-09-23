#ifndef __UART_COMM_H
#define __UART_COMM_H
#include <stdint.h>
int uart_open(const char *dev, int baudrate);
int uart_send_data(uint8_t *buf, int len);
int uart_recv_data(uint8_t *buf, int max_len);
void uart_close(void);
#endif
