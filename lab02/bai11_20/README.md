# LAB02 – Bài 11–20 – Tăng Ngọc Triển (SE200521 / Beooo1)

Nối tiếp trực tiếp bài 1–10 đã có trên nhánh `main` (mốc bắt đầu `58cfdbf`).
Code hoàn chỉnh nằm tại [../driver/lab2_driver.c](../driver/lab2_driver.c).
Đây là **một driver tích hợp**, không phải 10 module cùng tranh major 240.
Mỗi bài có một commit chức năng và một trang hướng dẫn riêng bên dưới.

## Tìm phần cần xem

| Bài | Nội dung | Chỗ chính trong driver | Hướng dẫn |
|---|---|---|---|
| 11 | Reset 5 bộ đếm | `reset_stats_store()` | [BAI_11.md](BAI_11.md) |
| 12 | Đếm open thành công an toàn | `lab2_open()` / `open_count_show()` | [BAI_12.md](BAI_12.md) |
| 13 | Khóa mutex khi đọc | `lab2_read()` và các hàm show | [BAI_13.md](BAI_13.md) |
| 14 | Ghi nối vào buffer | `lab2_write()` | [BAI_14.md](BAI_14.md) |
| 15 | Xóa bằng clear chính xác | `lab2_is_clear()` / `lab2_write()` | [BAI_15.md](BAI_15.md) |
| 16 | Điều chỉnh giới hạn ghi | `max_size_show/store()` | [BAI_16.md](BAI_16.md) |
| 17 | Kích thước lần ghi cuối | `last_write_size_show()` | [BAI_17.md](BAI_17.md) |
| 18 | Đầy đủ thống kê procfs | `lab2_proc_show()` | [BAI_18.md](BAI_18.md) |
| 19 | EOF đến khi có phiên bản dữ liệu mới | `struct lab2_reader`, `lab2_read()` | [BAI_19.md](BAI_19.md) |
| 20 | GET_BUFFER_LEN / CLEAR_BUFFER | `lab2_ioctl()` và `lab2_ioctl.h` | [BAI_20.md](BAI_20.md) |

Các chú thích bên phải trong driver được căn cùng cột. `lab2_ctl.c` là ví dụ userspace ngắn cho ioctl;
`test_lab2.c` là bộ kiểm tra tự động đầy đủ, không phải phần cần học thuộc.

## Quy ước khi ghép chung các bài

- Giữ tên `lab2`, major **240**, class `lab2_class`, `/proc/lab2_info`, version `1.0` và các sysfs cũ.
- Bài 14 đổi chính sách ghi sang **append** theo yêu cầu: hai lần ghi `abc`, `DEF` cho `abcDEF`.
- `buffer_size = 1024` là dung lượng cấp phát; `buffer_len` tối đa 1023 vì chừa NUL.
- `max_size` là giới hạn **mỗi lần ghi dữ liệu**, mặc định 128 và chỉ chỉnh trong 1..128 để giữ trần bài 4.
  Nếu write dài hơn giới hạn thì nhận một phần và trả số byte đã nhận. Không đủ chỗ chứa phần đó: ENOSPC, giữ nguyên dữ liệu.
- Chỉ chuỗi đúng `clear` hoặc `clear\n` là lệnh xóa. Lệnh này được xử lý nguyên vẹn dù max_size nhỏ hơn 6.
  `clearX`, chuỗi có NUL hoặc thêm ký tự khác vẫn là dữ liệu thường.
- `write_count`/`total_bytes_written` tính cả clear gửi qua write. Ghi 0 byte hoặc write lỗi không đổi thống kê/timestamp/thế hệ.
- `read_count` đếm mọi lần **callback lab2_read được gọi**, kể cả EOF/lỗi; tổng byte chỉ cộng khi trả dữ liệu thành công.
  Lỗi do VFS chặn trước callback không thể đếm trong driver. Đọc proc/sysfs không tăng read_count.
- Reset chỉ đặt open_count/read_count/write_count/tổng byte đọc/tổng byte ghi về 0; không xóa buffer và không đổi cấu hình,
  timestamp hoặc last_write_size. Đây là số lần open thành công **kể từ insmod hoặc reset**, không phải số fd đang mở.
- EOF thuộc từng file đang mở. Write mới làm fd cũ đọc lại từ đầu **toàn bộ phiên bản buffer mới**, gồm cả phần cũ do append.
  Không có writer thì cat kết thúc bình thường. `FMODE_ATOMIC_POS` giúp các thread dùng chung fd không đọc trùng vị trí.
- CLEAR_BUFFER qua ioctl không phải write: không tăng bộ đếm ghi, không đổi last_write_size/timestamp, vẫn tạo thế hệ mới.
- Dữ liệu và mọi trạng thái runtime mới đều dùng `lab2_mutex`; giữ atomic64 cũ của bài 1–10 để ít đổi code nền.
  `last_data`/proc hiển thị text nên dừng ở NUL giữa chuỗi; read/ioctl vẫn dùng buffer_len đúng cho dữ liệu nhị phân.

## Chạy bản có sẵn trong repo (không cần sudo trên Ubuntu)

Từ thư mục gốc repo:

```sh
# Chạy 3 vòng kiểm tra tự động rồi tắt QEMU
sh lab02/bai11_20/CHAY_QEMU.sh --test

# Hoặc vào shell QEMU để thử thủ công; nhấn Enter nếu được hỏi
sh lab02/bai11_20/CHAY_QEMU.sh
```

Trong QEMU, user là root nên không cần sudo:

```sh
printf clear > /dev/lab2
echo reset > /sys/class/lab2_class/lab2/reset_stats
printf abc > /dev/lab2
printf DEF > /dev/lab2
cat /dev/lab2                                      # abcDEF
cat /sys/class/lab2_class/lab2/buffer_len           # 6
cat /sys/class/lab2_class/lab2/last_write_size      # 3
cat /proc/lab2_info
/usr/bin/lab2_ctl len                             # Buffer length: 6
/usr/bin/lab2_ctl clear
cat /sys/class/lab2_class/lab2/status              # EMPTY
```

Thoát QEMU bằng Ctrl+A rồi X, hoặc gõ `reboot -f`. Bộ test xóa dữ liệu và reset thống kê.
Muốn chạy test thủ công trong guest: `/usr/bin/test_lab2.sh` (script sẽ gỡ module đang có rồi nạp bản mới).

Ảnh boot dành cho phần này là `lab02/output/zImage_bai11_20` (Linux 5.15.0 SMP ARM),
config kèm trong `kernel_bai11_20.config`, cùng DTB của LAB01. Kernel nền không sửa source,
nguồn [Linux 5.15](https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.tar.xz).
Ảnh `lab01/output/zImage` cũ được giữ nguyên: lần thử với ảnh đó không có console trước timeout,
chưa xác định nguyên nhân. Kết quả PASS bên dưới được đo bằng ảnh kernel riêng đã nêu.

## Build lại

Đặt repo ở đường dẫn không có khoảng trắng; cần cross-GCC ARM, QEMU ARM và source kernel đã build.

```sh
python3 lab02/bai11_20/BUILD_VA_DONG_GOI.py --kernel "$HOME/embedded_lab1/kernel/linux-5.15"
```

Script build `.ko`, 2 chương trình ARM, chép vào rootfs và tạo lại initramfs bằng `usr/gen_init_cpio`.
Không insmod lên host. Nếu đổi kernel/config, dùng module và ảnh kernel được build từ cùng kernel đó:

```sh
LAB2_KERNEL_IMAGE="$HOME/embedded_lab1/kernel/linux-5.15/arch/arm/boot/zImage" \
  sh lab02/bai11_20/CHAY_QEMU.sh --test
```

Chỉ build module:

```sh
make -C lab02/driver KDIR="$HOME/embedded_lab1/kernel/linux-5.15"
```

## Kết quả kiểm tra thực tế

- Cross-build module ARM Linux 5.15 với `-Werror`: PASS.
- Build module x86_64 bằng headers Ubuntu 7.0.0-38 với `-Werror`: PASS; **không nạp lên host**.
  Có thông báo công cụ build kernel/BTF khác môi trường; không có lỗi biên dịch module.
- QEMU ARM Cortex-A9, 512 MiB, SMP 2: **590 checks/vòng × 3 vòng**, mã cuối `BAI11_20_TEST_RESULT=0`.
- Mỗi vòng: 160 open đồng thời đếm đúng, 200 write nối không mất byte; đọc từng phần, EOF, reset,
  giới hạn ghi, buffer đầy, dữ liệu nhị phân, EFAULT/ENOSPC/EINVAL/ENOTTY và nhiều thread dùng chung fd.
- Đã thử read/write/clear/proc/reset đồng thời. delete_module bị từ chối khi fd còn mở;
  sau đóng fd thì gỡ được và proc/sysfs/device biến mất. Không thấy BUG/Oops/WARNING/KASAN trong log lifecycle.

Log console trong [logs/](logs/) (chỉ chuẩn hóa xuống dòng CRLF thành LF), bao gồm boot và kernel lifecycle đầy đủ. Đây là kiểm tra trong VM,
không thay thế kiểm chứng trên mọi kernel hoặc thiết bị thật. Bài có hỗ trợ AI soạn code/chú thích/test;
thành viên cần đọc, kiểm tra yêu cầu lớp và tự giải thích nội dung trước khi nộp.

Tài liệu tham chiếu: [ioctl](https://docs.kernel.org/driver-api/ioctl.html),
[sysfs](https://docs.kernel.org/filesystems/sysfs.html),
[mutex](https://docs.kernel.org/locking/mutex-design.html).
