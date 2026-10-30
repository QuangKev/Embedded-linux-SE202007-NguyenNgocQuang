# Mac dinh build ARM cho QEMU cua LAB01; co the ghi de bang make KDIR=... ARCH=...
KDIR ?= $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
obj-m += lab2_driver.o
ifeq ($(ARCH),arm)
ccflags-y += -march=armv7-a
endif

.PHONY: all clean
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
