#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "../driver/lab2_ioctl.h"

static int checks;
#define CHECK(expr) do { checks++; if (!(expr)) { \
    fprintf(stderr, "FAIL line %d: %s (errno=%d)\n", __LINE__, #expr, errno); exit(1); } } while (0)
#define SYS "/sys/class/lab2_class/lab2/"
static void text(const char *path, char *out, size_t cap) {
    int fd = open(path, O_RDONLY);
    ssize_t n;
    CHECK(fd >= 0); n = read(fd, out, cap - 1); CHECK(n >= 0);
    out[n] = 0; CHECK(close(fd) == 0);
}
static long long number(const char *path) {
    char value[128]; text(path, value, sizeof(value)); return strtoll(value, NULL, 10);
}
static void setting(const char *path, const char *value, int error) {
    int fd = open(path, O_WRONLY);
    ssize_t n;
    CHECK(fd >= 0); errno = 0; n = write(fd, value, strlen(value));
    if (error) CHECK(n == -1 && errno == error);
    else CHECK(n == (ssize_t)strlen(value));
    CHECK(close(fd) == 0);
}
static long long proc_number(const char *name) {
    char value[4096], *at;
    text("/proc/lab2_info", value, sizeof(value)); at = strstr(value, name);
    CHECK(at != NULL); at = strchr(at, ':'); CHECK(at != NULL);
    return strtoll(at + 1, NULL, 10);
}
static int device(void) { int fd = open("/dev/lab2", O_RDWR); CHECK(fd >= 0); return fd; }
static void clear(int fd) { CHECK(ioctl(fd, LAB2_CLEAR_BUFFER) == 0); }
static void reset(void) { setting(SYS "reset_stats", "reset\n", 0); }
static void data_is(const void *expected, size_t size) {
    int fd = device(); char data[1200]; ssize_t n = read(fd, data, sizeof(data));
    CHECK(n == (ssize_t)size); CHECK(memcmp(data, expected, size) == 0);
    CHECK(read(fd, data, sizeof(data)) == 0); CHECK(read(fd, data, 1) == 0);
    CHECK(close(fd) == 0);
}
static void *open_worker(void *unused) {
    int i; (void)unused;
    for (i = 0; i < 40; i++) {
        int fd = open("/dev/lab2", O_RDONLY);
        if (fd < 0 || close(fd)) return (void *)1;
    }
    return NULL;
}
static void *write_worker(void *unused) {
    int i, fd; (void)unused; fd = open("/dev/lab2", O_WRONLY);
    if (fd < 0) return (void *)1;
    for (i = 0; i < 50; i++) if (write(fd, "x", 1) != 1) { close(fd); return (void *)1; }
    return close(fd) ? (void *)1 : NULL;
}
static void parallel(void *(*worker)(void *)) {
    pthread_t threads[4]; void *result; int i;
    for (i = 0; i < 4; i++) CHECK(pthread_create(&threads[i], NULL, worker, NULL) == 0);
    for (i = 0; i < 4; i++) { CHECK(pthread_join(threads[i], &result) == 0); CHECK(result == NULL); }
}
static void *stress_writer(void *unused) {
    int i, fd; (void)unused; fd = open("/dev/lab2", O_RDWR);
    if (fd < 0) return (void *)1;
    for (i = 0; i < 120; i++) {
        if (ioctl(fd, LAB2_CLEAR_BUFFER) || write(fd, "abcdef", 6) != 6) { close(fd); return (void *)1; }
    }
    close(fd); return NULL;
}
static void *stress_reader(void *unused) {
    int i, fd; char buf[64]; ssize_t n; (void)unused;
    fd = open("/dev/lab2", O_RDONLY); if (fd < 0) return (void *)1;
    for (i = 0; i < 300; i++) {
        n = read(fd, buf, sizeof(buf));
        if (n < 0 || (n > 0 && (n != 6 || memcmp(buf, "abcdef", 6)))) { close(fd); return (void *)1; }
    }
    close(fd); return NULL;
}
static void *stress_stats(void *unused) {
    int i, fd; char buf[4096]; ssize_t n; (void)unused;
    for (i = 0; i < 120; i++) {
        fd = open("/proc/lab2_info", O_RDONLY); if (fd < 0) return (void *)1;
        n = read(fd, buf, sizeof(buf) - 1); close(fd); if (n < 0) return (void *)1;
        buf[n] = 0;
        if (!strstr(buf, "Read count") || !strstr(buf, "Status")) return (void *)1;
        fd = open(SYS "reset_stats", O_WRONLY); if (fd < 0) return (void *)1;
        n = write(fd, "reset", 5); close(fd); if (n != 5) return (void *)1;
    }
    return NULL;
}
struct shared_reader { int fd, count[256], total; };
static void *same_fd_reader(void *arg) {
    struct shared_reader *reader = arg;
    unsigned char byte;
    ssize_t n;
    while ((n = read(reader->fd, &byte, 1)) == 1) { reader->count[byte]++; reader->total++; }
    return n < 0 ? (void *)1 : NULL;
}
int main(void) {
    int fd, other, i;
    char buf[1200], attributes[128];
    __u32 length;
    long long before;
    pthread_t t[3]; void *result;
    setvbuf(stdout, NULL, _IOLBF, 0);
    fd = device();
    CHECK(number(SYS "buffer_size") == 1024); CHECK(number(SYS "max_size") == 128);
    CHECK(number(SYS "last_write_size") == 0);
    text(SYS "version", attributes, sizeof(attributes)); CHECK(!strcmp(attributes, "1.0\n"));
    text(SYS "status", attributes, sizeof(attributes)); CHECK(!strcmp(attributes, "EMPTY\n"));
    text(SYS "last_data", attributes, sizeof(attributes)); CHECK(!strcmp(attributes, "(empty)\n"));
    clear(fd); reset();
    CHECK(number(SYS "open_count") == 0); CHECK(number(SYS "read_count") == 0);
    CHECK(proc_number("Write count") == 0); CHECK(proc_number("Total bytes read") == 0);
    CHECK(proc_number("Total bytes written") == 0);
    setting(SYS "reset_stats", "bad", EINVAL);
    CHECK(write(fd, "abc", 3) == 3); CHECK(read(fd, buf, 2) == 2 && !memcmp(buf, "ab", 2));
    CHECK(read(fd, buf, 9) == 1 && buf[0] == 'c'); CHECK(read(fd, buf, 1) == 0);
    CHECK(number(SYS "read_count") == 3); CHECK(proc_number("Total bytes read") == 3);
    CHECK(proc_number("Write count") == 1); CHECK(proc_number("Total bytes written") == 3);
    reset(); CHECK(proc_number("Write count") == 0); CHECK(number(SYS "read_count") == 0);
    CHECK(number(SYS "buffer_len") == 3); CHECK(number(SYS "last_write_size") == 3);
    data_is("abc", 3); puts("PASS 11: reset exact counters; data and last size retained");

    reset(); parallel(open_worker); CHECK(number(SYS "open_count") == 160);
    puts("PASS 12: 160 concurrent successful opens counted exactly");

    clear(fd); CHECK(write(fd, "abc", 3) == 3);
    other = device(); CHECK(read(other, buf, 1) == 1 && buf[0] == 'a');
    errno = 0; CHECK(syscall(SYS_read, other, (void *)(uintptr_t)1, 1) == -1 && errno == EFAULT);
    CHECK(read(other, buf, 2) == 2 && !memcmp(buf, "bc", 2)); CHECK(close(other) == 0);
    before = proc_number("Total bytes written");
    errno = 0; CHECK(syscall(SYS_write, fd, (void *)(uintptr_t)1, 3) == -1 && errno == EFAULT);
    CHECK(proc_number("Total bytes written") == before); data_is("abc", 3);
    puts("PASS 13: EFAULT leaves buffer and read position intact");

    clear(fd); CHECK(write(fd, "abc", 3) == 3); CHECK(write(fd, "DEF", 3) == 3);
    data_is("abcDEF", 6);
    clear(fd); memset(buf, 'x', sizeof(buf)); CHECK(write(fd, buf, 200) == 128);
    CHECK(number(SYS "buffer_len") == 128); CHECK(number(SYS "last_write_size") == 128);
    clear(fd);
    for (i = 0; i < 7; i++) CHECK(write(fd, buf, 128) == 128);
    CHECK(write(fd, buf, 127) == 127); CHECK(number(SYS "buffer_len") == 1023);
    before = proc_number("Write count"); errno = 0;
    CHECK(write(fd, "x", 1) == -1 && errno == ENOSPC); CHECK(proc_number("Write count") == before);
    CHECK(number(SYS "last_write_size") == 127);
    data_is(buf, 1023); puts("PASS 14: append, 128-byte cap, exact capacity and ENOSPC");

    CHECK(write(fd, "clear\n", 6) == 6); CHECK(number(SYS "buffer_len") == 0);
    CHECK(write(fd, "clearX", 6) == 6); data_is("clearX", 6);
    CHECK(write(fd, "clear", 5) == 5); CHECK(number(SYS "buffer_len") == 0);
    CHECK(write(fd, "clear\nX", 7) == 7); data_is("clear\nX", 7);
    clear(fd); CHECK(write(fd, "clear\0", 6) == 6); data_is("clear\0", 6);
    puts("PASS 15: exact clear and clear-newline only; binary data supported");

    setting(SYS "max_size", "0", EINVAL); setting(SYS "max_size", "129", EINVAL);
    setting(SYS "max_size", "-1", EINVAL); setting(SYS "max_size", "abc", EINVAL);
    setting(SYS "max_size", "3x", EINVAL); setting(SYS "max_size", "999999999999999999999", EINVAL);
    CHECK(number(SYS "max_size") == 128);
    setting(SYS "max_size", "3\n", 0); clear(fd); CHECK(write(fd, "abcdef", 6) == 3);
    data_is("abc", 3); CHECK(number(SYS "last_write_size") == 3);
    setting(SYS "max_size", "1", 0); CHECK(number(SYS "buffer_len") == 3);
    CHECK(write(fd, "clear\n", 6) == 6); CHECK(number(SYS "last_write_size") == 6);
    CHECK(number(SYS "buffer_len") == 0); setting(SYS "max_size", "128", 0);
    puts("PASS 16: max_size validation, partial writes, clear exemption");

    CHECK(write(fd, "abcd", 4) == 4); CHECK(number(SYS "last_write_size") == 4);
    before = proc_number("Write count"); CHECK(write(fd, "", 0) == 0);
    CHECK(number(SYS "last_write_size") == 4); CHECK(proc_number("Write count") == before);
    puts("PASS 17: last successful size; zero write leaves it unchanged");
    CHECK(proc_number("Data length") == 4); CHECK(proc_number("Last write size") == 4);
    text("/proc/lab2_info", buf, sizeof(buf)); CHECK(strstr(buf, "Status         : DATA") != NULL);
    CHECK(strstr(buf, "Last write time: never") == NULL);
    before = number(SYS "read_count"); CHECK(proc_number("Read count") == before);
    CHECK(number(SYS "read_count") == before); puts("PASS 18: proc snapshot includes all counters and status");

    clear(fd); CHECK(write(fd, "abc", 3) == 3); other = device();
    CHECK(read(other, buf, 1) == 1 && buf[0] == 'a'); CHECK(read(other, buf, 8) == 2 && !memcmp(buf, "bc", 2));
    CHECK(read(other, buf, 8) == 0); CHECK(read(other, buf, 8) == 0);
    CHECK(write(fd, "D", 1) == 1); CHECK(read(other, buf, 8) == 4 && !memcmp(buf, "abcD", 4));
    CHECK(read(other, buf, 8) == 0); CHECK(write(fd, "", 0) == 0); CHECK(read(other, buf, 8) == 0);
    clear(fd); CHECK(read(other, buf, 8) == 0); CHECK(write(fd, "Z", 1) == 1);
    CHECK(read(other, buf, 8) == 1 && buf[0] == 'Z'); data_is("Z", 1);
    CHECK(close(other) == 0); puts("PASS 19: partial reads, repeated EOF and same-fd new-generation reads");

    {
        struct shared_reader readers[4] = {0};
        pthread_t threads[4];
        int j, seen, total = 0;
        clear(fd); for (i = 0; i < 128; i++) buf[i] = (char)i;
        CHECK(write(fd, buf, 128) == 128); other = device();
        for (i = 0; i < 4; i++) {
            readers[i].fd = other;
            CHECK(pthread_create(&threads[i], NULL, same_fd_reader, &readers[i]) == 0);
        }
        for (i = 0; i < 4; i++) {
            CHECK(pthread_join(threads[i], &result) == 0); CHECK(result == NULL); total += readers[i].total;
        }
        CHECK(total == 128);
        for (i = 0; i < 128; i++) {
            seen = 0; for (j = 0; j < 4; j++) seen += readers[j].count[i]; CHECK(seen == 1);
        }
        CHECK(close(other) == 0);
        puts("PASS shared fd: four readers receive each byte exactly once");
        clear(fd); CHECK(write(fd, "Z", 1) == 1);
    }

    CHECK(ioctl(fd, LAB2_GET_BUFFER_LEN, &length) == 0 && length == 1);
    errno = 0; CHECK(ioctl(fd, LAB2_GET_BUFFER_LEN, (void *)(uintptr_t)1) == -1 && errno == EFAULT);
    errno = 0; CHECK(ioctl(fd, _IO('L', 99)) == -1 && errno == ENOTTY);
    errno = 0; CHECK(ioctl(fd, _IOR('L', 1, uint64_t), &length) == -1 && errno == ENOTTY);
    before = proc_number("Write count"); clear(fd);
    CHECK(proc_number("Write count") == before); CHECK(ioctl(fd, LAB2_GET_BUFFER_LEN, &length) == 0 && length == 0);
    text(SYS "status", attributes, sizeof(attributes)); CHECK(!strcmp(attributes, "EMPTY\n"));
    puts("PASS 20: shared ioctl ABI, bad pointer/command and clear semantics");

    clear(fd); reset(); parallel(write_worker); CHECK(number(SYS "buffer_len") == 200);
    CHECK(proc_number("Write count") == 200); CHECK(proc_number("Total bytes written") == 200);
    memset(buf, 'x', 200); data_is(buf, 200); puts("PASS concurrent append: 200 writes, no lost bytes");
    clear(fd); CHECK(write(fd, "abcdef", 6) == 6);
    CHECK(pthread_create(&t[0], NULL, stress_writer, NULL) == 0);
    CHECK(pthread_create(&t[1], NULL, stress_reader, NULL) == 0);
    CHECK(pthread_create(&t[2], NULL, stress_stats, NULL) == 0);
    for (i = 0; i < 3; i++) { CHECK(pthread_join(t[i], &result) == 0); CHECK(result == NULL); }
    puts("PASS mixed read/write/clear/proc/reset concurrency");
    errno = 0;
    CHECK(syscall(SYS_delete_module, "lab2_driver", O_NONBLOCK) == -1 && (errno == EBUSY || errno == EWOULDBLOCK));
    puts("PASS module reference: delete_module rejected while fd is open");
    CHECK(close(fd) == 0);
    printf("ALL TESTS PASSED: %d checks\n", checks);
    return 0;
}
