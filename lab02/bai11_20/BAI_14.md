# Bài 14 – Append
Ghi lần sau được nối sau dữ liệu cũ. Mỗi write nhận tối đa 128 byte; cả buffer chứa tối đa 1023 byte + NUL.
Không đủ chỗ cho phần dữ liệu đã giới hạn: trả ENOSPC, không thay buffer/thống kê.
```sh
printf clear > /dev/lab2
printf abc > /dev/lab2
printf DEF > /dev/lab2
cat /dev/lab2  # abcDEF
```
Lệnh clear của bài 3 vẫn được giữ. Zero-length write trả 0 và không thay trạng thái.
