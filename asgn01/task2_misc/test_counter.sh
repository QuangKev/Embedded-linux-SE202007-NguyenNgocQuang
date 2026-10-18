#!/bin/sh
# test_counter.sh - Kiểm thử counter_driver trong QEMU
# Author: NguyenNgocQuang - SE202007V

KO=/lib/modules/5.15.0/counter_driver.ko
DEV=/dev/counter
PASS=0; FAIL=0

check() {
    if [ "$2" -eq 0 ]; then echo "  [PASS] $1"; PASS=$((PASS+1))
    else echo "  [FAIL] $1"; FAIL=$((FAIL+1)); fi
}

echo '========= ASSIGNMENT-01 Task 2: misc counter test ========='

echo '[1] Load module'
lsmod | grep -q counter_driver && rmmod counter_driver
insmod $KO; check "insmod" $?
lsmod | grep counter
# Nếu không có devtmpfs: lấy minor từ /sys rồi tạo node thủ công
if [ ! -c $DEV ]; then
    MINOR=$(cut -d: -f2 /sys/class/misc/counter/dev)
    mknod $DEV c 10 $MINOR
fi
ls -la $DEV
grep -w counter /proc/misc

echo '[2] /proc/counter_info - BEFORE write'
cat /proc/counter_info

echo '[3] Auto-increment on read'
A=$(cat $DEV); B=$(cat $DEV); C=$(cat $DEV)
echo "  reads: $A $B $C"
[ "$A $B $C" = "0 1 2" ]; check "read returns 0,1,2" $?

echo '[4] Write 100'
echo 100 > $DEV
V=$(cat $DEV); echo "  read: $V"
[ "$V" = "100" ]; check "set to 100" $?
V=$(cat $DEV); [ "$V" = "101" ]; check "next read = 101" $?

echo '[5] /proc/counter_info - AFTER write'
cat /proc/counter_info

echo '[6] Reset with 0'
echo 0 > $DEV
V=$(cat $DEV); echo "  read: $V"
[ "$V" = "0" ]; check "reset to 0" $?

echo '[7] Invalid input'
echo -5  > $DEV 2>/dev/null; [ $? -ne 0 ]; check "negative rejected (EINVAL)" $?
echo abc > $DEV 2>/dev/null; [ $? -ne 0 ]; check "non-number rejected (EINVAL)" $?

echo '[8] /proc/counter_info - AFTER reset'
cat /proc/counter_info

echo '[9] Unload module'
rmmod counter_driver; check "rmmod" $?
[ ! -e /proc/counter_info ]; check "/proc entry removed" $?
dmesg | grep counter: | tail -20

echo "========== RESULT: $PASS passed, $FAIL failed =========="
[ $FAIL -eq 0 ]
