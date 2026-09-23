export ARCH=arm
export CROSS_COMPILE=/home/linux/rk3506_linux6.1_sdk_v1.2.0/prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-

CFLAGS = -Wall -Wextra

TARGET = rk3506_comm
OBJS = main.o uart_comm.o protocol_pack.o protocol_parse.o business.o hw_driver.o hw_touch.o crc_util.o

all: $(OBJS)
	$(CROSS_COMPILE)gcc $(OBJS) -o $(TARGET)

%.o:%.c
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
