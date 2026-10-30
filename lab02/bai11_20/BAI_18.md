# Bài 18 – Mở rộng procfs
Giữ toàn bộ dòng cũ; thêm Read count, Status, Max write size và Last write size.
Read count/write count/tổng byte đọc ghi và trạng thái được xuất trong cùng vùng mutex.
```sh
cat /proc/lab2_info
```
Đọc proc/sysfs không tăng read_count của character device.
