# Bài 20 – ioctl
Header dùng chung `lab02/driver/lab2_ioctl.h` định nghĩa:
- `LAB2_GET_BUFFER_LEN`: truyền địa chỉ biến __u32, nhận độ dài hiện tại; thành công trả 0.
- `LAB2_CLEAR_BUFFER`: không có tham số; xóa buffer, đánh dấu thế hệ mới cho bài 19.
Sai lệnh trả ENOTTY; GET với con trỏ không hợp lệ trả EFAULT.
Clear bằng ioctl không phải write, do đó không tăng write_count/tổng byte ghi và không đổi last_write_size/timestamp.
Tất cả thay đổi buffer/generation dưới lab2_mutex. Có compat_ioctl cho userspace 32-bit trên kernel 64-bit.
Nguồn: https://docs.kernel.org/driver-api/ioctl.html
