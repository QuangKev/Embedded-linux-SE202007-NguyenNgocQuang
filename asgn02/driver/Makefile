# Makefile - cross-compile sms_sensor_driver for ARMv7 (vexpress-a9)
# Author: Nguyen Ngoc Quang - SE202007
# Chinh KDIR cho dung thu muc kernel 5.15 da build o LAB-01 / ASSIGNMENT-01

KDIR          ?= $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH          ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m := sms_sensor_driver.o

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
