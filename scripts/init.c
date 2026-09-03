// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 阶段 6 最小回归 init（ARM64，静态，无 busybox）。
 *
 * 只做一件事：证明 Kernel → initramfs → /init → userspace 已打通，
 * 并保持空闲不崩溃。不碰 USB/configfs/fork，用于隔离定位：
 * 若此版本稳定黑屏不重启，说明先前的花屏/关停机是诊断 init 的
 * USB(configfs/UDC) 操作触发。
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>

static void w(const char *p, const char *s)
{
    int fd = open(p, O_WRONLY);
    if (fd < 0)
        return;
    (void)write(fd, s, strlen(s));
    close(fd);
}

static void log_all(const char *s)
{
    int fd;
    fd = open("/dev/kmsg", O_WRONLY);
    if (fd >= 0) { (void)write(fd, s, strlen(s)); close(fd); }
    fd = open("/dev/console", O_WRONLY);
    if (fd >= 0) { (void)write(fd, s, strlen(s)); close(fd); }
}

int main(void)
{
    (void)mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755");
    (void)mkdir("/proc", 0555);
    (void)mkdir("/sys", 0755);
    (void)mount("proc", "/proc", "proc", 0, NULL);
    (void)mount("sysfs", "/sys", "sysfs", 0, NULL);

    log_all("\n======== THYME_STAGE6_USERSPACE_REACHED ========\n");
    log_all("[init] minimal idle init running (PID=1), no USB ops\n");

    {
        int fd = open("/proc/cmdline", O_RDONLY);
        char b[2048] = {0};
        if (fd >= 0) { read(fd, b, sizeof(b) - 1); close(fd); }
        log_all("[init] cmdline: ");
        log_all(b);
        log_all("\n");
    }

    /* 保活，避免 init 退出触发 panic */
    for (;;)
        sleep(3600);
    return 0;
}