# LOS201 – Embedded Linux Systems

Repository nộp bài thực hành (LAB) và bài tập cá nhân (ASSIGNMENT) môn **Embedded Linux Systems (LOS201)** – FPT University.

| | |
|---|---|
| **Sinh viên** | Nguyễn Ngọc Quang | |**MSSV**| |SE202007|
                | Trần Gia Khánh | |**MSSV**| |SE200963|
                | Tăng Ngọc Triển | |**MSSV**| |SE200521|

---

## Mục lục

- [Môi trường thực hiện](#môi-trường-thực-hiện)
- [Cấu trúc repository](#cấu-trúc-repository)
- [Tổng quan bài nộp](#tổng-quan-bài-nộp)
- [LAB-01 – Kernel Configuration & Embedded Linux Boot](#lab-01--kernel-configuration--embedded-linux-boot)
- [LAB-02 – Device Driver & File System Management](#lab-02--device-driver--file-system-management)
- [LAB-03 – Debugging & Performance Analysis](#lab-03--debugging--performance-analysis)
- [ASSIGNMENT-01 – How to Write a Device Driver](#assignment-01--how-to-write-a-device-driver)
- [ASSIGNMENT-02 – Sensor Monitoring System](#assignment-02--sensor-monitoring-system)
- [Build & chạy lại](#build--chạy-lại)
- [Ghi chú](#ghi-chú)

---

## Môi trường thực hiện

| Thành phần | Cấu hình |
|---|---|
| Host | Ubuntu Linux x86_64 |
| Target | ARMv7 Cortex-A9 |
| Emulator | QEMU Versatile Express (`vexpress-a9`) |
| Kernel target | Linux 5.15 (`vexpress_defconfig`) |
| Toolchain | `arm-linux-gnueabihf` (kernel + module build bằng `gcc-12`) |
| Root filesystem | BusyBox initramfs (`cpio.gz`), init qua `/etc/inittab` + `rcS` |
| Công cụ debug/profiling | `gdb-multiarch`, `gdbserver` (ARM), Valgrind, strace, perf, cyclictest, gnuplot |

---

## Cấu trúc repository

```
.
├── README.md
├── lab01/      # Kernel configuration, BusyBox rootfs, boot QEMU
├── lab02/      # Character driver, procfs/sysfs, MTD/JFFS2, BusyBox init
├── lab03/      # GDB remote debug, Valgrind, strace/perf, cyclictest
├── asgn01/     # ioctl driver + misc device driver
└── asgn02/     # Sensor Monitoring System (kernel driver + ứng dụng đa luồng)
```

Mỗi thư mục bài nộp đều có thư mục `bao_cao/` chứa báo cáo PDF.

---

## Tổng quan bài nộp

| Bài | Nội dung chính | Thư mục | Tag nộp bài | Trạng thái |
|---|---|---|---|---|
| LAB-01 | Kernel config, cross-compile, BusyBox rootfs, boot QEMU | [`lab01/`](lab01/) | `lab01-submit` | Hoàn thành |
| LAB-02 | Char driver, procfs/sysfs, MTD/JFFS2, BusyBox init | [`lab02/`](lab02/) | `lab02-submit` | Hoàn thành |
| LAB-03 | GDB, Valgrind, strace, perf, cyclictest | [`lab03/`](lab03/) | `lab03-submit` | Hoàn thành |
| ASSIGNMENT-01 | ioctl interface, misc device driver | [`asgn01/`](asgn01/) | `asgn01-submit` | Hoàn thành |
| ASSIGNMENT-02 | Sensor Monitoring System | [`asgn02/`](asgn02/) | `asgn02-submit` | Hoàn thành |

Xem đúng phiên bản đã nộp của từng bài:

```bash
git checkout <tag>        # ví dụ: git checkout asgn02-submit
```

---

## LAB-01 – Kernel Configuration & Embedded Linux Boot

**Phạm vi bài thực hành**

- Cấu hình và cross-compile Linux kernel 5.15 cho ARM (`vexpress_defconfig`).
- Tìm hiểu bootloader, Device Tree Blob (`vexpress-v2p-ca9.dtb`) và quá trình boot.
- Xây dựng BusyBox root filesystem, đóng gói initramfs và boot trên QEMU.

**Thư mục**

| Đường dẫn | Nội dung |
|---|---|
| `lab01/configs/` | File cấu hình kernel / BusyBox |
| `lab01/output/` | Kernel image và DTB đã build |
| `lab01/rootfs/` | Root filesystem BusyBox |
| `lab01/bao_cao/` | Báo cáo LAB-01 |

Browse: [`lab01/`](lab01/)

---

## LAB-02 – Device Driver & File System Management

**Phạm vi bài thực hành**

- **Phần A:** Viết character device driver `lab2_driver` (open / read / write / release, có mutex bảo vệ buffer), node `/dev/lab2` (major 240).
- **Phần B:** Giao diện procfs `/proc/lab2_info` và sysfs `/sys/class/lab2_class/lab2/` (`buffer_len`, `open_count`, `last_data`).
- **Phần C:** MTD subsystem với `nandsim`, tạo và mount JFFS2, kiểm tra dữ liệu còn nguyên sau umount/mount.
- **Phần D:** Tích hợp BusyBox init (`inittab`, `rcS`) để tự động nạp module khi boot QEMU.

**Thư mục**

| Đường dẫn | Nội dung |
|---|---|
| `lab02/driver/` | Mã nguồn `lab2_driver.c` và `Makefile` |
| `lab02/rootfs/` | initramfs có `rcS`, `inittab`, script `test_driver.sh`, `test_procfs.sh` |
| `lab02/output/` | initramfs đã đóng gói |
| `lab02/logs/` | `boot_log.txt`, `driver_test.txt`, `procfs_test.txt`, `mtd_jffs2.txt` |
| `lab02/bao_cao/` | Báo cáo LAB-02 |

**Kết quả kiểm chứng**

| Hạng mục | Kết quả |
|---|---|
| Nạp module tự động khi boot | PASS |
| `test_driver.sh` (ghi/đọc `/dev/lab2`) | PASS |
| `test_procfs.sh` (procfs + sysfs) | PASS |
| JFFS2 trên nandsim: ghi, umount, mount lại, dữ liệu còn nguyên | PASS |

Browse: [`lab02/`](lab02/)

---

## LAB-03 – Debugging & Performance Analysis

**Phạm vi bài thực hành**

- **Phần A:** Remote debug ứng dụng ARM trong QEMU bằng `gdbserver` + `gdb-multiarch` (breakpoint, watchpoint, backtrace, registers).
- **Phần B:** Phát hiện và sửa memory leak bằng Valgrind; đo bộ nhớ qua `/proc/PID/status` trên QEMU.
- **Phần C:** Phân tích system call bằng strace và CPU hotspot bằng perf (`record`, `report`, `stat`).
- **Phần D:** Đo scheduling latency bằng cyclictest, vẽ histogram bằng gnuplot, so sánh lý thuyết với PREEMPT_RT.

**Thư mục**

| Đường dẫn | Nội dung |
|---|---|
| `lab03/apps/` | `buggy_app.c`, `leaky_app.c`, `perf_target.c` và các bản build |
| `lab03/output/` | `initramfs_lab3.cpio.gz` |
| `lab03/logs/` | GDB session, Valgrind (trước/sau khi sửa), strace, perf, histogram cyclictest |
| `lab03/bao_cao/` | Báo cáo LAB-03 |

**Kết quả chính**

| Phần | Kết quả |
|---|---|
| A – GDB | Tìm đủ 3 bug: array out-of-bounds, use-after-free, integer overflow |
| B – Valgrind | Trước khi sửa: 16.900 byte bị leak trong 250 block. Sau khi sửa: `0 errors from 0 contexts` |
| C – strace/perf | `perf_target` là CPU-bound: `log()` + `heavy_compute` chiếm khoảng 96% mẫu perf |
| D – cyclictest | Host kernel (PREEMPT_DYNAMIC): Min/Avg/Max ≈ 1 / 2 / 142 µs |

Browse: [`lab03/`](lab03/)

---

## ASSIGNMENT-01 – How to Write a Device Driver

**Phạm vi bài tập**

- **Task 1:** Mở rộng character driver của LAB-02 thành `asgn1_driver` với ioctl interface (magic `'q'`): `RESET_BUFFER`, `GET_STATS`, `SET_MODE`, `GET_VERSION`. Công cụ userspace `asgn1_ctl` gọi ioctl.
- **Task 2:** Misc device driver `counter_driver` (`/dev/counter`, `atomic_t` counter, `/proc/counter_info`).
- Phân tích thiết kế, so sánh `register_chrdev()` với `misc_register()`.

**Thư mục**

| Đường dẫn | Nội dung |
|---|---|
| `asgn01/task1_ioctl/` | `asgn1_driver.c`, `asgn1_ioctl.h`, `asgn1_ctl.c`, `test_asgn1.sh`, `Makefile` |
| `asgn01/task2_misc/` | `counter_driver.c`, `test_counter.sh`, `Makefile` |
| `asgn01/logs/` | Log dmesg, phiên QEMU và output của hai test script |
| `asgn01/bao_cao/` | Báo cáo ASSIGNMENT-01 |

**Kết quả kiểm chứng**

| Driver | Device | Test script | Kết quả |
|---|---|---|---|
| `asgn1_driver` | `/dev/asgn1` (major 241) | `test_asgn1.sh` | 9/9 PASS |
| `counter_driver` | `/dev/counter` (misc, major 10) | `test_counter.sh` | 9/9 PASS |

Các trường hợp lỗi cũng được kiểm tra: ioctl sai magic trả `ENOTTY`, `SET_MODE` giá trị không hợp lệ trả `EINVAL`.

Browse: [`asgn01/`](asgn01/)

---

## ASSIGNMENT-02 – Sensor Monitoring System

Bài tập capstone: hệ thống giám sát cảm biến chạy trên ARMv7 QEMU, gồm kernel driver mô phỏng cảm biến và ứng dụng userspace đa luồng.

### Kiến trúc

```
┌──────────────────── Sensor Monitoring System ─────────────────────┐
│                                                                    │
│  [Sensor Thread] ──► circular buffer ──► [Processor Thread]        │
│        ▲  read() + ioctl                      │                    │
│        │                                      ├──► alarm_queue (mq)│
│  sms_sensor_driver.ko                         │        ▼           │
│   /dev/sms_sensor                             │  [Alert Thread]    │
│   /proc/sms_stats                             ▼        │           │
│                                       [Logger Thread] ◄┘           │
│                                          └──► /tmp/sms.log         │
└────────────────────────────────────────────────────────────────────┘
```

### Thành phần

| Thành phần | Vai trò |
|---|---|
| `sms_sensor_driver.ko` | Misc device `/dev/sms_sensor`, trả về dòng CSV `timestamp_ms,temperature,humidity`; thống kê qua `/proc/sms_stats`; ioctl `SMS_SET_INTERVAL` |
| Sensor Thread | Đọc driver theo chu kỳ (mặc định 500 ms), `SCHED_FIFO` priority 50, `clock_nanosleep(TIMER_ABSTIME)`, `mlockall()` |
| Processor Thread | Rolling average 10 mẫu, phát cảnh báo `TEMP_HIGH`, `HUMID_HIGH`, `TEMP_SPIKE` |
| Alert Thread | Nhận cảnh báo qua POSIX message queue, in ra stderr và đếm theo loại |
| Logger Thread | Ghi `/tmp/sms.log` có timestamp, flush mỗi 5 giây, rotate khi > 1 MB |
| Circular buffer | Cấp phát một lần trên heap, bảo vệ bằng `pthread_mutex` + `pthread_cond` |

Tùy chọn dòng lệnh của `sms_app`: `-i <ms>` chu kỳ đọc, `-t <°C>` ngưỡng nhiệt độ, `-h <%>` ngưỡng độ ẩm, `-d <sec>` thời gian chạy, `-v` verbose. Ctrl+C (SIGINT) dừng sạch toàn bộ thread, flush log và in tổng kết.

### Thư mục

| Đường dẫn | Nội dung |
|---|---|
| `asgn02/driver/` | `sms_sensor_driver.c`, `sms_sensor.h`, `Makefile` |
| `asgn02/app/` | Mã nguồn ứng dụng chia theo thread, circular buffer, `Makefile` (`make arm`, `make host`) |
| `asgn02/output/` | initramfs boot được trên QEMU |
| `asgn02/logs/` | Log chạy mẫu, `sms.log`, `/proc/sms_stats` tại 3 thời điểm, memory footprint, Valgrind, strace, perf, GDB, `jitter_report.txt` |
| `asgn02/demo/` | Video demo |
| `asgn02/bao_cao/` | Báo cáo kỹ thuật ASSIGNMENT-02 |

### Task 1 & 2 – Ứng dụng đa luồng, bộ nhớ và tích hợp driver

| Hạng mục | Kết quả |
|---|---|
| Build module ARMv7, nạp trên QEMU | PASS |
| `/dev/sms_sensor`, `/proc/sms_stats`, ioctl `SMS_SET_INTERVAL` | PASS |
| Chạy 60 s, interval 500 ms | `read_count` = 119 (yêu cầu ≈ 120 ± 10), exit code 0 |
| Memory footprint (T = 5 / 30 / 55 s) | VmRSS giữ nguyên 1744 kB, không tăng theo thời gian |
| Valgrind (`sms_app_x86`, `--leak-check=full`) | 0 bytes definitely lost |
| Graceful shutdown (SIGINT) | Tất cả thread join, log được flush |

### Task 3 – Debug & Profiling

- **GDB:** debug đa luồng (`info threads`, `thread apply all bt`, `set scheduler-locking on`, breakpoint có điều kiện).
- **Valgrind:** kiểm tra leak trên bản build x86.
- **strace -c:** phân tích tần suất syscall theo từng thao tác của ứng dụng.
- **perf:** xác định CPU hotspot và đề xuất tối ưu.

### Task 4 – Real-Time Scheduling

Kernel target được build lại với `CONFIG_HZ=1000`, `CONFIG_HIGH_RES_TIMERS=y` và `CONFIG_PREEMPT=y`. Với kernel mặc định (`HZ=100`, không có high-resolution timer), jitter đo được khoảng 30 ms và `SCHED_FIFO` không khác `SCHED_OTHER`.

So sánh jitter của Sensor Thread khi có 4 tiến trình tải CPU chạy song song:

| Metric | SCHED_OTHER | SCHED_FIFO (prio 50) |
|---|---:|---:|
| Average jitter (µs) | 243.0 | 157.6 |
| P99 jitter (µs) | 413 | 215 |
| Max jitter (µs) | 523 | 583 |

- `SCHED_FIFO` giảm jitter trung bình khoảng 35% và P99 khoảng 48% so với `SCHED_OTHER`.
- Lần chạy chính (500 ms, 60 s): Max jitter 629 µs, dưới 1 ms, đáp ứng tốt cho chu kỳ đọc cảm biến 500 ms.
- Chi tiết số mẫu và histogram: [`asgn02/logs/jitter_report.txt`](asgn02/logs/jitter_report.txt).

### Task 5 – Báo cáo & Demo

- Báo cáo kỹ thuật: kiến trúc hệ thống, state machine, phân tích đồng bộ thread và chứng minh không deadlock, kết quả đo lường, reflection.
- Video demo: boot QEMU, nạp driver, chạy `sms_app -v`, cảnh báo, xem log, graceful shutdown.

Browse: [`asgn02/`](asgn02/)

---

## Build & chạy lại

Build kernel module (ví dụ ASSIGNMENT-02):

```bash
cd asgn02/driver
make arm          # đường dẫn kernel source trong Makefile có thể cần chỉnh lại
```

Build ứng dụng:

```bash
cd asgn02/app
make arm          # binary ARM cho QEMU
make host         # binary x86_64 cho Valgrind / perf
```

Boot QEMU:

```bash
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -nographic \
  -kernel <zImage> \
  -dtb <vexpress-v2p-ca9.dtb> \
  -initrd <initramfs.cpio.gz> \
  -append "console=ttyAMA0,115200 rdinit=/sbin/init"
```

Thoát QEMU: `Ctrl+A` rồi `X`.

---

## Ghi chú

- Module phải được build với đúng kernel đang boot (kiểm tra bằng `modinfo <file>.ko | grep vermagic`). Kernel dùng cho LAB-03 và ASSIGNMENT-02 bật `PREEMPT`, nên module của LAB-02 đã được build lại để khớp.
- Kernel source, file `.o` và các file build trung gian không được đưa vào repository.
- Chỉ các kết quả đã được kiểm chứng bằng log mới được đánh dấu PASS. Log gốc nằm trong thư mục `logs/` của từng bài.
