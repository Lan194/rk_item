#include "uart_comm.h"
#include "log_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

static int uart_fd = -1;

int uart_open(const char *dev, int baudrate)
{
    (void)baudrate;
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if(fd < 0)
    {
        perror("uart open fail");
        return -1;
    }
    struct termios opt;
    tcgetattr(fd, &opt);
    cfsetispeed(&opt, B115200);
    cfsetospeed(&opt, B115200);

    opt.c_cflag |= CS8;
    opt.c_cflag &= ~CSTOPB;
    opt.c_cflag &= ~PARENB;
    opt.c_cflag &= ~CRTSCTS; /*关闭硬件流控，文档要求无流控*/

    opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    opt.c_oflag &= ~OPOST;

    opt.c_cc[VTIME] = 5;
    opt.c_cc[VMIN] = 0;
    tcsetattr(fd, TCSANOW, &opt);
    uart_fd = fd;
    return fd;
}

int uart_send_data(uint8_t *buf, int len)
{
    if(uart_fd <0 || buf == NULL || len <=0)
        return -1;
    return write(uart_fd, buf, len);
}

int uart_recv_data(uint8_t *buf, int max_len)
{
    if(uart_fd <0 || buf == NULL)
        return -1;
    memset(buf,0,max_len);
    return read(uart_fd, buf, max_len);
}

void uart_close(void)
{
    if(uart_fd >0)
    {
        close(uart_fd);
        uart_fd = -1;
    }
}
