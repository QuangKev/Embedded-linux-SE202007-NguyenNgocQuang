# Bài 15 – Clear chính xác
Đã có clear ở bài 3; bài này hoàn thiện cùng append: chỉ `clear` hoặc `clear\n` xóa buffer.
`clearX`, `clear\nX`, chuỗi chứa NUL hay chuỗi dài khác đều là dữ liệu thường.
So với độ dài gốc của write, không nhận nhầm sau khi giới hạn kích thước.
```sh
echo clear > /dev/lab2
cat /sys/class/lab2_class/lab2/status  # EMPTY
```
Copy_from_user lỗi không xóa/sửa buffer. Clear qua write vẫn tính là một write thành công.
