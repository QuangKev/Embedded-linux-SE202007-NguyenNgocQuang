# Bài 11 – Reset thống kê
Ghi `reset` (có hoặc không có newline) vào sysfs `reset_stats` để đặt 5 bộ đếm về 0.
Giữ nguyên dữ liệu, giới hạn ghi, timestamp và kích thước lần ghi cuối.
```sh
echo reset > /sys/class/lab2_class/lab2/reset_stats
cat /proc/lab2_info
```
Chuỗi khác trả EINVAL. Code tham khảo được chuẩn bị với hỗ trợ AI; cần đọc và tự giải thích khi vấn đáp.
