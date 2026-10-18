/*
 * asgn1_ctl.c - Công cụ userspace gọi ioctl tới /dev/asgn1
 * (shell không gọi được ioctl() nên cần chương trình C này)
 * Author : NguyenNgocQuang - SE202007V
 * Usage  : asgn1_ctl reset | stats | mode <0|1> | version | info | bad
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "asgn1_ioctl.h"

#define DEV_PATH "/dev/asgn1"

static void usage(const char *p)
{
	fprintf(stderr, "Usage: %s reset | stats | mode <0|1> | version | info | bad\n", p);
}

/* In mã ioctl đã giải mã - dùng cho báo cáo Task 3 */
static void print_cmd(const char *name, unsigned int cmd)
{
	printf("  %-20s = 0x%08x  type='%c' nr=%u size=%u dir=%u\n",
	       name, cmd, _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));
}

int main(int argc, char *argv[])
{
	int fd, ret = 0;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	/* "info" không cần mở device */
	if (strcmp(argv[1], "info") == 0) {
		printf("ioctl command encoding (dir: 0=none 1=write 2=read):\n");
		print_cmd("ASGN1_RESET_BUFFER", ASGN1_RESET_BUFFER);
		print_cmd("ASGN1_GET_STATS",    ASGN1_GET_STATS);
		print_cmd("ASGN1_SET_MODE",     ASGN1_SET_MODE);
		print_cmd("ASGN1_GET_VERSION",  ASGN1_GET_VERSION);
		return 0;
	}

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror("open " DEV_PATH);
		return 1;
	}

	if (strcmp(argv[1], "reset") == 0) {
		if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) { perror("RESET_BUFFER"); ret = 1; }
		else printf("RESET_BUFFER: OK\n");

	} else if (strcmp(argv[1], "stats") == 0) {
		struct asgn1_stats st;
		if (ioctl(fd, ASGN1_GET_STATS, &st) < 0) { perror("GET_STATS"); ret = 1; }
		else {
			printf("GET_STATS:\n");
			printf("  open_count      : %d\n", st.open_count);
			printf("  write_count     : %d\n", st.write_count);
			printf("  read_count      : %d\n", st.read_count);
			printf("  buffer_len      : %d\n", st.buffer_len);
			printf("  last_write_size : %d\n", st.last_write_size);
			printf("  mode            : %d (%s)\n", st.mode,
			       st.mode == ASGN1_MODE_KERNEL ? "KERNEL" : "ECHO");
		}

	} else if (strcmp(argv[1], "mode") == 0 && argc == 3) {
		int32_t m = atoi(argv[2]);
		if (ioctl(fd, ASGN1_SET_MODE, &m) < 0) { perror("SET_MODE"); ret = 1; }
		else printf("SET_MODE: %d OK\n", m);

	} else if (strcmp(argv[1], "version") == 0) {
		char ver[ASGN1_VERSION_LEN];
		if (ioctl(fd, ASGN1_GET_VERSION, ver) < 0) { perror("GET_VERSION"); ret = 1; }
		else {
			ver[ASGN1_VERSION_LEN - 1] = '\0';
			printf("GET_VERSION: %s\n", ver);
		}

	} else if (strcmp(argv[1], "bad") == 0) {
		/* Gửi lệnh không tồn tại để kiểm tra driver trả -ENOTTY */
		if (ioctl(fd, _IO(ASGN1_MAGIC, 99)) < 0 && errno == ENOTTY)
			printf("Invalid ioctl correctly rejected (ENOTTY)\n");
		else { printf("ERROR: invalid ioctl not rejected\n"); ret = 1; }

	} else {
		usage(argv[0]);
		ret = 1;
	}

	close(fd);
	return ret;
}
