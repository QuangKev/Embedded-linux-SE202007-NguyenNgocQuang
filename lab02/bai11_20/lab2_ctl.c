#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "../driver/lab2_ioctl.h"

int main(int argc, char **argv) {
    int fd, ret;
    __u32 length;
    if (argc != 2 || (strcmp(argv[1], "len") && strcmp(argv[1], "clear"))) {
        fprintf(stderr, "Cach dung: %s len|clear\n", argv[0]); return 1;
    }
    fd = open("/dev/lab2", O_RDWR);                                                                     // Mo thiet bi truoc khi goi ioctl
    if (fd == -1) { perror("open"); return 1; }
    if (!strcmp(argv[1], "len")) {
        ret = ioctl(fd, LAB2_GET_BUFFER_LEN, &length);                                                  // Truyen dia chi de kernel ghi ket qua vao
        if (ret == 0) printf("Buffer length: %u\n", length);
    } else {
        ret = ioctl(fd, LAB2_CLEAR_BUFFER);                                                             // Lenh khong co du lieu di kem
        if (ret == 0) printf("Da xoa buffer.\n");
    }
    if (ret == -1) perror("ioctl");
    close(fd);                                                                                          // Dong fd tren ca nhanh thanh cong va loi ioctl
    return ret == -1 ? 1 : 0;
}
