# Bài 16 – max_size đọc/ghi
`max_size` giới hạn byte dữ liệu nhận trong một write, không phải tổng buffer_len.
Cho phép 1..128 để giữ mức trần của bài 4; mặc định 128. Ngoài khoảng hoặc không phải số: EINVAL.
Ghi dài hơn giới hạn trả số byte đã nhận (partial write); ứng dụng có thể gọi tiếp.
Giảm max_size không cắt dữ liệu cũ. Lệnh điều khiển clear/clear\n được nhận nguyên vẹn, kể cả max_size < 6.
```sh
echo 3 > /sys/class/lab2_class/lab2/max_size
printf clear > /dev/lab2
printf abc > /dev/lab2
cat /dev/lab2  # abc
echo 128 > /sys/class/lab2_class/lab2/max_size
```
