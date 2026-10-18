#!/bin/sh
# test_asgn1.sh - Kiểm thử asgn1_driver trong QEMU
# Author: NguyenNgocQuang - SE202007V

KO=/lib/modules/5.15.0/asgn1_driver.ko
DEV=/dev/asgn1
MAJOR=241
PASS=0; FAIL=0

check() {   # check "<tên test>" <mã trả về: 0 = đúng>
    if [ "$2" -eq 0 ]; then echo "  [PASS] $1"; PASS=$((PASS+1))
    else echo "  [FAIL] $1"; FAIL=$((FAIL+1)); fi
}

echo '=========== ASSIGNMENT-01 Task 1: ioctl test ==========='

echo '[1] Load module'
lsmod | grep -q asgn1_driver && rmmod asgn1_driver
insmod $KO; check "insmod" $?
lsmod | grep asgn1
[ -c $DEV ] || mknod $DEV c $MAJOR 0
ls -la $DEV

echo '[2] ioctl encoding'
asgn1_ctl info

echo '[3] GET_VERSION'
asgn1_ctl version; check "GET_VERSION" $?

echo '[4] Echo mode (default)'
echo 'Hello Echo' > $DEV
OUT=$(cat $DEV); echo "  read: [$OUT]"
[ "$OUT" = "Hello Echo" ]; check "echo mode stores data as-is" $?

echo '[5] Kernel mode'
asgn1_ctl mode 0
echo 'Hello Kernel' > $DEV
OUT=$(cat $DEV); echo "  read: [$OUT]"
[ "$OUT" = "[KERNEL] Hello Kernel" ]; check "kernel mode adds prefix" $?

echo '[6] Invalid mode / invalid command'
asgn1_ctl mode 5 2>/dev/null; [ $? -ne 0 ]; check "SET_MODE 5 rejected (EINVAL)" $?
asgn1_ctl bad; check "unknown ioctl -> ENOTTY" $?
asgn1_ctl mode 1

echo '[7] GET_STATS'
asgn1_ctl stats; check "GET_STATS" $?

echo '[8] RESET_BUFFER'
echo 'data before reset' > $DEV
asgn1_ctl reset
OUT=$(cat $DEV); echo "  read after reset: [$OUT]"
[ -z "$OUT" ]; check "buffer empty after reset" $?
asgn1_ctl stats

echo '[9] Unload module'
rmmod asgn1_driver; check "rmmod" $?
dmesg | grep asgn1 | tail -25

echo "========== RESULT: $PASS passed, $FAIL failed =========="
[ $FAIL -eq 0 ]
