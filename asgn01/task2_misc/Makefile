# Makefile - ASSIGNMENT-01 Task 2 (cross-compile ARMv7)
KDIR          ?= $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH          := arm
CROSS_COMPILE := arm-linux-gnueabihf-
KCC           := $(CROSS_COMPILE)gcc-12

obj-m += counter_driver.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CC=$(KCC) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CC=$(KCC) clean

.PHONY: all clean
