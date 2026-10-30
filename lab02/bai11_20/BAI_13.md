# Bài 13 – Mutex cho read
`lab2_read()` khóa từ lúc kiểm tra buffer_len đến khi copy và cập nhật offset/thống kê.
Mọi nhánh lỗi và EOF đều unlock. Các hàm sysfs/proc đọc biến chung cũng dùng cùng mutex.
`copy_to_user()` có thể sleep; mutex cho phép sleep, khác spinlock.
Read_count vẫn đếm mọi lần gọi read như bài 1; total_bytes_read chỉ cộng byte trả thành công.
