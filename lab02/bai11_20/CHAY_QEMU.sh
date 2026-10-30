#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
extra=''
[ "${1:-}" != --test ] || extra='bai11_20_test=1'
export QEMU_AUDIO_DRV=none
exec qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic -no-reboot \
    -kernel "${LAB2_KERNEL_IMAGE:-lab02/output/zImage_bai11_20}" -dtb lab01/output/vexpress-v2p-ca9.dtb \
    -initrd lab02/output/initramfs_lab2.cpio.gz \
    -append "console=ttyAMA0,115200 rdinit=/init mem=512M $extra"
