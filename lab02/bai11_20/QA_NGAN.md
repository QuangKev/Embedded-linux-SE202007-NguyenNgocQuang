# Câu hỏi vấn đáp ngắn – Bài 11–20

| Câu hỏi | Trả lời ngắn |
|---|---|
| Buffer là gì? | Vùng nhớ chứa dữ liệu. device_buffer có 1024 ô byte. |
| buffer_len khác buffer_size thế nào? | len là số byte đang có; size là sức chứa đã cấp phát. |
| max_size là gì? | Số byte dữ liệu tối đa được nhận trong một write, ở bài này 1..128. |
| Append là gì? | Nối dữ liệu mới sau phần cũ: memcpy(device_buffer + buffer_len, ...). |
| Vì sao chừa 1 byte? | Đặt '\0' để các hàm xuất text không đọc vượt dữ liệu. |
| Mutex làm gì? | Cho một luồng vào vùng dữ liệu chung tại một thời điểm. |
| Vì sao atomic64 rồi vẫn mutex? | Atomic giữ một phép đếm an toàn; mutex giúp 5 bộ đếm reset và được đọc cùng trạng thái với buffer. |
| read_count có tăng khi EOF? | Có, nếu callback read được gọi; tổng byte đọc thì không tăng khi trả 0. |
| EOF là gì? | read trả 0, báo đã hết dữ liệu hiện tại. |
| Offset là gì? | Vị trí byte sẽ đọc tiếp trên một file đang mở. |
| Tại sao phải cập nhật offset? | Để lần read sau lấy phần còn lại; nếu cứ đọc từ 0 thì cat không kết thúc. |
| generation là gì? | Số phiên bản dữ liệu, tăng sau write/clear; fd phát hiện phiên bản mới thì đọc lại từ đầu. |
| private_data là gì? | Chỗ driver lưu con trỏ trạng thái riêng của một file đang mở. |
| Hai fd có cùng offset không? | Hai lần open riêng có offset riêng; dup hoặc các thread dùng chung fd thì dùng chung file/offset. |
| FMODE_ATOMIC_POS để làm gì? | Nhờ VFS tuần tự hóa thao tác vị trí khi nhiều thread dùng chung file. |
| show/store là gì? | show tạo text khi đọc sysfs; store kiểm tra và xử lý text khi ghi sysfs. |
| DEVICE_ATTR_RO / RW? | Tạo thuộc tính sysfs chỉ đọc / đọc ghi bằng các hàm show/store tương ứng. |
| Vì sao không gán trực tiếp con trỏ user? | Địa chỉ thuộc user-space có thể không hợp lệ; phải copy_to_user/copy_from_user hoặc helper tương ứng. |
| memdup_user là gì? | Cấp buffer kernel và copy dữ liệu user vào; lỗi thì trả con trỏ mã lỗi, kiểm tra bằng IS_ERR/PTR_ERR. |
| Vì sao copy vào temp trước? | Nếu copy lỗi thì không làm hỏng device_buffer đang dùng. |
| memcmp(...)=0 nghĩa gì? | Các byte được so giống nhau; còn phải so đúng độ dài để clearX không thành lệnh clear. |
| seq_file là gì? | Cơ chế kernel hỗ trợ xuất báo cáo text qua proc, quản lý đọc từng phần/EOF. |
| ioctl là gì? | Gửi lệnh điều khiển đến driver ngoài thao tác read/write thông thường. |
| _IOR và _IO nghĩa gì? | _IOR: kernel trả dữ liệu cho user qua con trỏ; _IO: lệnh không kèm dữ liệu ở bài này. |
| Vì sao dùng __u32? | Kiểu luôn 32 bit, giữ ABI ioctl ổn định giữa app 32/64 bit. |
| EINVAL / EFAULT? | Tham số không hợp lệ / truy cập bộ nhớ user không hợp lệ. |
| ENOSPC / ENOTTY? | Buffer không đủ chỗ / driver không hỗ trợ mã ioctl đó. |
| Vì sao write trả 3 không phải 0? | Read/write thành công trả số byte; open và ioctl của bài này thành công trả 0. |
| Reset có xóa dữ liệu không? | Không; chỉ 5 bộ đếm. Muốn xóa dùng clear hoặc CLEAR_BUFFER. |
| Tại sao close không giảm open_count? | Đây là tổng số lần mở thành công từ lần reset, không phải số fd đang mở. |
| Tại sao rmmod bị từ chối khi fd mở? | .owner = THIS_MODULE giữ tham chiếu module, tránh tháo code đang được dùng. |
| Tại sao init lỗi phải dọn ngay? | Kernel không gọi exit để dọn một module đã init thất bại; phải hủy những gì tạo thành công trước lỗi. |
