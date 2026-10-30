# Bài 17 – last_write_size
Sysfs chỉ đọc. Giá trị ban đầu 0, sau write thành công là số byte đã nhận (có tính newline).
Clear qua write có kích thước 5 hoặc 6. Zero-write/lỗi không đổi giá trị này.
Reset thống kê bài 11 chỉ reset 5 bộ đếm được yêu cầu, không reset last_write_size/timestamp.
```sh
printf abc > /dev/lab2
cat /sys/class/lab2_class/lab2/last_write_size  # 3
```
