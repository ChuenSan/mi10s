// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 最小 ARM64 initramfs init（阶段 6 bring-up 用）。
 *
 * 目标：在内核被 ABL 拉起后，挂载基础文件系统、打印可辨识的早期日志，
 * 然后无限循环保持运行，避免 init 退出触发 panic 导致难以观察。
 *
 * 编译（CI 内）：aarch64-linux-gnu-gcc -static -Os -s init.c -o init
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static void log_all(const char *msg)
{
    int fd;

    /* /dev/console（串口/屏），仅尽力 */
    fd = open("/dev/console", O_WRONLY);
    if (fd >= 0) {
        (void)write(fd, msg, strlen(msg));
        close(fd);
    }
    /* /dev/kmsg（内核环形缓冲，pstore/ramoops 兜底） */
    fd = open("/dev/kmsg", O_WRONLY);
    if (fd >= 0) {
        (void)write(fd, msg, strlen(msg));
        close(fd);
    }
}

int main(void)
{
    /* 尽量放行孤儿、避免任何早期僵死 */
    if (fork() > 0)
        for (;;)
            pause();

    /* 挂载基础文件系统 */
    (void)mount("devtmpfs", "/dev", "devtmpfs",
                0, "mode=0755");
    (void)mkdir("/proc", 0555);
    (void)mkdir("/sys", 0755);
    (void)mkdir("/tmp", 01777);
    (void)mount("proc", "/proc", "proc", 0, NULL);
    (void)mount("sysfs", "/sys", "sysfs", 0, NULL);
    (void)mount("tmpfs", "/tmp", "tmpfs", 0, "mode=01777");

    log_all("\n=== thyme mainline kernel: /init running ===\n"
            "stage6 bring-up initramfs reached userspace\n");

    /* 打印内核 cmdline，便于确认 ABL→内核传参 */
    {
        char buf[2048] = {0};
        int fd = open("/proc/cmdline", O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                char out[2200];
                int l = snprintf(out, sizeof(out),
                                 "cmdline: %s\n", buf);
                log_all(out);
            }
            close(fd);
        }
    }

    log_all("=== /init done, idling (kernel alive) ===\n");

    /* 保持运行：让内核保持活着，方便串口/pstore 观察 */
    for (;;)
        sleep(3600);

    return 0; /* unreachable */
}