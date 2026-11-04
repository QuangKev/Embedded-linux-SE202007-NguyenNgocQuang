# LOS201 – Embedded Linux Systems

Repository nộp bài LAB và ASSIGNMENT môn **Embedded Linux Systems (LOS201)** – FPT University.

| Họ và tên | MSSV |
|---|---|
| Nguyễn Ngọc Quang | SE202007 |
| Tăng Ngọc Triển | SE200521 |
| Trần Gia Khánh | SE200963 |

## Môi trường

- **Host:** Ubuntu Linux x86_64
- **Target:** ARMv7 Cortex-A9, QEMU `vexpress-a9`
- **Kernel:** Linux 5.15
- **Toolchain:** `arm-linux-gnueabihf`
- **Rootfs:** BusyBox initramfs

## Tổng quan

| Bài | Nội dung | Thư mục | Tag |
|---|---|---|---|
| LAB-01 | Kernel Configuration & Embedded Linux Boot | [`lab01/`](lab01/) | `lab01-submit` |
| LAB-02 | Device Driver & File System Management | [`lab02/`](lab02/) | `lab02-submit` |
| LAB-03 | Debugging & Performance Analysis | [`lab03/`](lab03/) | `lab03-submit` |
| ASSIGNMENT-01 | How to Write a Device Driver | [`asgn01/`](asgn01/) | `asgn01-submit` |
| ASSIGNMENT-02 | Sensor Monitoring System | [`asgn02/`](asgn02/) | `asgn02-submit` |

Mỗi bài có thư mục `logs/` chứa log kiểm chứng và `bao_cao/` chứa báo cáo PDF.

---

## LAB-01 – Kernel Configuration & Embedded Linux Boot

- Cấu hình và cross-compile Linux kernel cho ARM.
- Tìm hiểu bootloader, Device Tree Blob và quá trình boot.
- Xây dựng BusyBox root filesystem, initramfs và boot trên QEMU.

**Thư mục:** `configs/`, `output/`, `rootfs/`, `bao_cao/`

---

## LAB-02 – Device Driver & File System Management

- Character device driver `lab2_driver` (`/dev/lab2`): open, read, write, release.
- Giao diện procfs `/proc/lab2_info` và sysfs `/sys/class/lab2_class/lab2/`.
- MTD với `nandsim` và filesystem JFFS2.
- BusyBox init tự động nạp module khi boot.

**Thư mục:** `driver/`, `rootfs/`, `output/`, `logs/`, `bao_cao/`

**Kết quả:** `test_driver.sh` PASS, `test_procfs.sh` PASS. Dữ liệu trên JFFS2 vẫn còn sau khi umount rồi mount lại.

---

## LAB-03 – Debugging & Performance Analysis

| Công cụ | Kết quả |
|---|---|
| GDB remote debug | Tìm được 3 bug: array out-of-bounds, use-after-free, integer overflow |
| Valgrind | Trước khi sửa leak 16.900 byte trong 250 block; sau khi sửa còn 0 lỗi |
| strace / perf | Ứng dụng CPU-bound, `log()` và `heavy_compute` chiếm khoảng 96% mẫu |
| cyclictest | Latency trên host: Min 1 µs, Avg 2 µs, Max 142 µs |

**Thư mục:** `apps/`, `output/`, `logs/`, `bao_cao/`

---

## ASSIGNMENT-01 – How to Write a Device Driver

| Task | Driver | Kết quả |
|---|---|---|
| Task 1 – ioctl | `asgn1_driver`: `RESET_BUFFER`, `GET_STATS`, `SET_MODE`, `GET_VERSION` | 9/9 PASS |
| Task 2 – misc device | `counter_driver`: `/dev/counter`, `/proc/counter_info` | 9/9 PASS |

Báo cáo có phần phân tích thiết kế và so sánh `register_chrdev()` với `misc_register()`.

**Thư mục:** `task1_ioctl/`, `task2_misc/`, `logs/`, `bao_cao/`

---

## ASSIGNMENT-02 – Sensor Monitoring System

Hệ thống giám sát cảm biến trên QEMU ARM, gồm một kernel driver và một ứng dụng 4 luồng.

- **Driver `sms_sensor`:** cung cấp `/dev/sms_sensor` (dữ liệu CSV nhiệt độ/độ ẩm) và `/proc/sms_stats`. Ioctl `SMS_SET_INTERVAL` đặt chu kỳ đọc.
- **Sensor Thread:** đọc driver theo chu kỳ với `SCHED_FIFO`.
- **Processor Thread:** tính trung bình trượt và phát cảnh báo.
- **Alert Thread:** nhận cảnh báo qua POSIX message queue.
- **Logger Thread:** ghi `/tmp/sms.log`.
- **Circular buffer:** bảo vệ bằng mutex và condition variable.

| Hạng mục | Kết quả |
|---|---|
| Chạy 60 s với chu kỳ 500 ms | `read_count` = 119, exit code 0 |
| Memory footprint | VmRSS giữ 1744 kB ở T = 5, 30 và 55 s |
| Valgrind | 0 bytes definitely lost |
| Debug & profiling | GDB đa luồng, strace, perf |
| SIGINT | Tất cả thread dừng sạch, log được flush |

**Real-time:** kernel được build lại với `HZ=1000`, `HIGH_RES_TIMERS` và `PREEMPT`. Kết quả đo khi có tải CPU:

| Jitter (µs) | SCHED_OTHER | SCHED_FIFO |
|---|---:|---:|
| Average | 243.0 | 157.6 |
| P99 | 413 | 215 |
| Max | 523 | 583 |

`SCHED_FIFO` giảm P99 khoảng 48% so với `SCHED_OTHER`.

**Thư mục:** `driver/`, `app/`, `output/`, `logs/`, `demo/`, `bao_cao/`

---

## Ghi chú

- Chỉ những kết quả đã kiểm chứng bằng log mới được ghi PASS. Log gốc nằm trong thư mục `logs/` của từng bài.
- Kernel module phải được build với đúng kernel đang boot, nếu không sẽ lỗi `vermagic`. Kernel của LAB-03 và ASSIGNMENT-02 có bật `PREEMPT`, nên module của LAB-02 đã được build lại cho khớp.
- Repository không chứa kernel source và các file build trung gian (`.o`, `.mod`, `.cmd`).
- Đường dẫn kernel source trong các `Makefile` có thể phải chỉnh lại theo máy build.
