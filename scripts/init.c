// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 阶段 6 诊断 initramfs init（ARM64，静态编译，无 busybox 依赖）。
 *
 * 目标：无串口条件下，证明 Kernel → initramfs → /init → userspace 已打通，
 * 并优先通过 USB gadget（ACM 串口）建立可观察通道。
 *
 * 行为：
 *   1. 挂 devtmpfs/proc/sysfs/configfs/tmpfs
 *   2. 向 /dev/kmsg 写入明显标记 THYME_STAGE6_USERSPACE_REACHED
 *   3. 枚举 /sys/class/udc；若有 UDC，用 configfs 建 ACM 串口 gadget 并绑定
 *   4. 打印 dmesg 尾部 + /proc/cmdline + udc 列表
 *   5. 若 /dev/ttyGS0 出现，起一个极简命令行回环（dmesg/cmdline/ls/reboot）
 *   6. 保活（无限 sleep），避免 init 退出触发 panic
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

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
    int fd, tty;
    /* kmsg（内核环形缓冲，可进 pstore/后续读） */
    fd = open("/dev/kmsg", O_WRONLY);
    if (fd >= 0) { (void)write(fd, s, strlen(s)); close(fd); }
    /* console（尽量） */
    tty = open("/dev/console", O_WRONLY);
    if (tty >= 0) { (void)write(tty, s, strlen(s)); close(tty); }
}

/* 建 configfs ACM 串口 gadget。返回 1 成功绑定，0 失败 */
static int setup_acm(const char *udc)
{
    char p[256], v[256];
    int ok = 1;

    /* 基础目录 */
    (void)mkdir("/sys/kernel/config/usb_gadget/g1", 0755);
    w("/sys/kernel/config/usb_gadget/g1/idVendor",  "0x18d1");
    w("/sys/kernel/config/usb_gadget/g1/idProduct", "0x4d11");
    w("/sys/kernel/config/usb_gadget/g1/bcdDevice", "0x0100");
    w("/sys/kernel/config/usb_gadget/g1/bcdUSB",    "0x0200");
    (void)mkdir("/sys/kernel/config/usb_gadget/g1/strings/0x409", 0755);
    w("/sys/kernel/config/usb_gadget/g1/strings/0x409/serialnumber", "THYME6");
    w("/sys/kernel/config/usb_gadget/g1/strings/0x409/manufacturer", "thyme-mainline");
    w("/sys/kernel/config/usb_gadget/g1/strings/0x409/product",      "Stage6 ACM");
    (void)mkdir("/sys/kernel/config/usb_gadget/g1/configs/c.1", 0755);
    w("/sys/kernel/config/usb_gadget/g1/configs/c.1/MaxPower", "500");
    (void)mkdir("/sys/kernel/config/usb_gadget/g1/configs/c.1/strings/0x409", 0755);
    w("/sys/kernel/config/usb_gadget/g1/configs/c.1/strings/0x409/configuration", "ACM");

    /* ACM 串口 function */
    (void)mkdir("/sys/kernel/config/usb_gadget/g1/functions/acm.0", 0755);
    /* 符号链接 function → 配置 */
    if (symlink("/sys/kernel/config/usb_gadget/g1/functions/acm.0",
                "/sys/kernel/config/usb_gadget/g1/configs/c.1/acm.0") < 0)
        ok = 0;

    /* 绑定 UDC（关键一步：让控制器真正枚举到宿主机） */
    snprintf(p, sizeof(p), "%s", udc);
    w("/sys/kernel/config/usb_gadget/g1/UDC", udc);

    snprintf(v, sizeof(v), "\n[init] ACM gadget bind UDC=%s ok=%d\n", udc, ok);
    log_all(v);
    return ok;
}

static void cmd_loop(const char *ttydev)
{
    FILE *f;
    char line[256];
    int i;

    /* 等 ttyGS0 出现 */
    for (i = 0; i < 40; i++) {
        if (access(ttydev, F_OK) == 0)
            break;
        sleep(1);
    }

    f = fopen(ttydev, "r+");
    if (!f) {
        log_all("[init] ACM tty not ready (no ttyGS0), keep probing\n");
        return;
    }
    fputs("\r\n=== THYME stage6 shell ===\r\n# ", f);
    fflush(f);

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, "dmesg", 5) == 0) {
            int fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
            char b[4096];
            ssize_t n;
            if (fd >= 0) {
                while ((n = read(fd, b, sizeof(b))) > 0)
                    fwrite(b, 1, n, f);
                close(fd);
            }
        } else if (strncmp(line, "cmdline", 7) == 0) {
            int fd = open("/proc/cmdline", O_RDONLY);
            char b[2048] = {0};
            if (fd >= 0) { read(fd, b, sizeof(b) - 1); close(fd); }
            fprintf(f, "%s\r\n", b);
        } else if (strncmp(line, "ls", 2) == 0) {
            DIR *d = opendir("/dev");
            struct dirent *e;
            if (d) {
                while ((e = readdir(d)))
                    if (e->d_name[0] != '.')
                        fprintf(f, "%s ", e->d_name);
                closedir(d);
                fputs("\r\n", f);
            }
        } else if (strncmp(line, "reboot", 6) == 0) {
            sync();
            reboot(RB_AUTOBOOT);
        } else if (strlen(line) > 0) {
            fprintf(f, "? %s\r\n", line);
        }
        fputs("# ", f);
        fflush(f);
    }
}

int main(void)
{
    /* 收养孤儿、拒绝僵死 */
    signal(SIGCHLD, SIG_IGN);
    if (fork() > 0)
        for (;;) pause();

    (void)mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755");
    (void)mkdir("/proc", 0555);
    (void)mkdir("/sys", 0755);
    (void)mkdir("/tmp", 01777);
    (void)mkdir("/sys/kernel/config", 0755);
    (void)mount("proc", "/proc", "proc", 0, NULL);
    (void)mount("sysfs", "/sys", "sysfs", 0, NULL);
    (void)mount("tmpfs", "/tmp", "tmpfs", 0, "mode=01777");
    /* configfs：USB gadget 的绑定接口 */
    (void)mount("configfs", "/sys/kernel/config", "configfs", 0, NULL);

    log_all("\n\n======== THYME_STAGE6_USERSPACE_REACHED ========\n");
    log_all("[init] minimal diagnostic init running (PID=1)\n");

    /* 打印 cmdline */
    {
        int fd = open("/proc/cmdline", O_RDONLY);
        char b[2048] = {0};
        if (fd >= 0) { read(fd, b, sizeof(b) - 1); close(fd); }
        log_all("[init] cmdline: \n");
        log_all(b);
        log_all("\n");
    }

    /* 枚举 UDC */
    {
        DIR *d = opendir("/sys/class/udc");
        struct dirent *e;
        char first_udc[64] = {0};
        int got = 0;
        if (d) {
            log_all("[init] /sys/class/udc present:\n");
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                log_all("  udc: ");
                log_all(e->d_name);
                log_all("\n");
                if (!got) { snprintf(first_udc, sizeof(first_udc), "%s", e->d_name); got = 1; }
            }
            closedir(d);
        } else {
            log_all("[init] /sys/class/udc MISSING (no gadget controller)\n");
        }

        if (got) {
            log_all("[init] building ACM gadget...\n");
            setup_acm(first_udc);
        }
    }

    log_all("[init] === diagnosis done, entering idle/command loop ===\n");
    log_all("[init] IF ACM enumerated: plug USB, open /dev/tty.usbmodem* on host\n");

    /* ACM tty /dev/ttyGS0 若有，起命令回环 */
    cmd_loop("/dev/ttyGS0");

    /* 保活 */
    for (;;)
        sleep(3600);
    return 0;
}