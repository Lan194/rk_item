export ARCH=arm
export CROSS_COMPILE=/home/linux/rk3506_linux6.1_sdk_v1.2.0/prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-

obj-m += pwm_backlight.o
obj-m += gpio_buzzer.o
obj-m += spi_touch.o

KDIR := /home/linux/rk3506_linux6.1_sdk_v1.2.0/kernel-6.1/
PWD ?= $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

