# Makefile - sms_app: ARMv7 target (make arm) + x86_64 host (make host)
# Author: Nguyen Ngoc Quang - SE202007
#
#   make arm    -> sms_app      (static ARM binary for the QEMU initramfs)
#   make host   -> sms_app_x86  (for Valgrind / perf / GDB on the PC)
#   make        -> both
#   make clean

CC_HOST ?= gcc
CC_ARM  ?= arm-linux-gnueabihf-gcc
OPT     ?= -O2

# -g and frame pointers: needed for gdb, valgrind line numbers and perf -g
CFLAGS  = -std=gnu11 $(OPT) -g -fno-omit-frame-pointer -Wall -Wextra -D_GNU_SOURCE -I../driver
LDLIBS  = -pthread -lrt

SRCS = sms_app.c sms_common.c circular_buffer.c sensor_thread.c \
       processor_thread.c alert_thread.c logger_thread.c
HDRS = sms_common.h circular_buffer.h sensor_thread.h \
       processor_thread.h alert_thread.h logger_thread.h ../driver/sms_sensor.h

.PHONY: all host arm clean

all: host arm

host: sms_app_x86
arm:  sms_app

sms_app_x86: $(SRCS) $(HDRS)
	$(CC_HOST) $(CFLAGS) -o $@ $(SRCS) $(LDLIBS)

# static: the initramfs has no shared libraries
sms_app: $(SRCS) $(HDRS)
	$(CC_ARM) $(CFLAGS) -static -o $@ $(SRCS) $(LDLIBS)

clean:
	rm -f sms_app sms_app_x86
