# Makefile - ASSIGNMENT-01 Task 1 (cross-compile ARMv7)
KDIR          ?= $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH          := arm
CROSS_COMPILE := arm-linux-gnueabihf-
KCC           := $(CROSS_COMPILE)gcc-12

obj-m += asgn1_driver.o

all: module ctl

module:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CC=$(KCC) modules

ctl: asgn1_ctl.c asgn1_ioctl.h
	$(CROSS_COMPILE)gcc -static -Wall -Wextra -o asgn1_ctl asgn1_ctl.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CC=$(KCC) clean
	rm -f asgn1_ctl

.PHONY: all module ctl clean
