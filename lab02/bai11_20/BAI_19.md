# Bài 19 – Read một lần trên mỗi fd
Mỗi open có `generation` riêng trong file->private_data. Read từng phần tăng offset; hết dữ liệu trả EOF.
Write/clear thành công tăng data_generation dưới mutex. Read tiếp trên fd cũ phát hiện thế hệ mới và bắt đầu từ 0.
Vì bài 14 dùng append, phiên bản dữ liệu mới gồm cả byte cũ + byte vừa nối, nên fd đọc lại toàn bộ phiên bản mới.
EOF không xóa buffer toàn cục; process khác vẫn đọc được. Zero-write và write lỗi không tạo thế hệ mới.
Đây không phải read chờ/blocking. Cat có thể nhận thêm dữ liệu nếu có writer đang liên tục thay buffer.
