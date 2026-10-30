# Bài 12 – Đếm open thành công
`open_count` được tăng dưới mutex và không giảm khi close. Sau reset, đếm từ 0 trở lại.
Nếu bộ đếm chạm INT_MAX, open trả EOVERFLOW thay vì gây tràn số nguyên có dấu.
```sh
echo reset > /sys/class/lab2_class/lab2/reset_stats
cat /dev/lab2 >/dev/null
cat /sys/class/lab2_class/lab2/open_count
```
Kết quả 1; đọc sysfs không mở `/dev/lab2` nên không tăng bộ đếm này.
