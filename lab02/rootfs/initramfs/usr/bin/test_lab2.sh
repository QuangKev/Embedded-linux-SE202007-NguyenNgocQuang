#!/bin/sh
# Chay trong QEMU/root; script se xoa du lieu va reset thong ke cua lab2.
set -eu
module=/lib/modules/$(uname -r)/lab2_driver.ko
dmesg -n 1
if grep -q '^lab2_driver ' /proc/modules; then rmmod lab2_driver; fi
echo '=== LAB02 BAI 11-20: start ==='
uname -a
dmesg -c >/dev/null
for cycle in 1 2 3; do
    echo "=== Load/test/unload cycle $cycle ==="
    insmod "$module"
    [ -c /dev/lab2 ] || mknod /dev/lab2 c 240 0
    /usr/bin/test_lab2
    printf abc > /dev/lab2
    /usr/bin/lab2_ctl len
    /usr/bin/lab2_ctl clear
    rmmod lab2_driver
    [ ! -e /proc/lab2_info ]
    [ ! -e /sys/class/lab2_class/lab2 ]
    [ ! -e /dev/lab2 ]
    echo "PASS unload cycle $cycle: proc/sysfs/device removed"
done
echo '=== Kernel lifecycle log ==='
dmesg
if dmesg | grep -E 'BUG:|Oops:|WARNING:|KASAN:|kernel panic'; then
    echo 'FAIL: kernel diagnostic detected'; exit 1
fi
echo 'ALL BAI 11-20 LIFECYCLE TESTS PASSED'
