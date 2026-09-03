// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 第一次 Runtime bring-up 诊断 init（ARM64 静态，无 busybox）。
 *
 * 目标：确立「Kernel → DTB → initramfs → /init → userspace」链路，
 * 并尝试建立 USB gadget（ACM 优先，ECM 兜底）作为唯一可观察通道。
 *
 * 行为：
 *   1. mount devtmpfs/proc/sysfs/configfs
 *   2. 向 /dev/kmsg 写 THYME_USERSPACE_REACHED
 *   3. 枚举 /sys/class/udc，打印到 /dev/console
 *   4. 若有 UDC，建 configfs USB gadget（ACM 优先）
 *   5. 打印 /proc/cmdline
 *   6. 保活循环（sleep），不自动退出
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>

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
    int fd = open("/dev/kmsg", O_WRONLY);
    if (fd >= 0) {
        (void)write(fd, s, strlen(s));
        close(fd);
    }
    fd = open("/dev/console", O_WRONLY);
    if (fd >= 0) {
        (void)write(fd, s, strlen(s));
        close(fd);
    }
}

/* 写 gadget 配置值：把 base 路径 + 子路径拼接后写 value */
static void gw(const char *base, const char *sub, const char *val)
{
    char path[256];
    snprintf(path, sizeof(path), "%s%s", base, sub);
    w(path, val);
}

/* 建目录：base 路径 + 子路径拼接 */
static void gmkdir(const char *base, const char *sub)
{
    char path[256];
    snprintf(path, sizeof(path), "%s%s", base, sub);
    (void)mkdir(path, 0755);
}

/* 建最简 USB gadget：ACM 优先，失败回落 ECM。返回 1 绑定成功 */
static int setup_gadget(const char *udc)
{
    const char *g = "/sys/kernel/config/usb_gadget/g1";

    (void)mkdir(g, 0755);
    gw(g, "/idVendor",  "0x18d1");
    gw(g, "/idProduct", "0x4d11");
    gw(g, "/bcdDevice", "0x0100");
    gw(g, "/bcdUSB",    "0x0200");
    gmkdir(g, "/strings/0x409");
    gw(g, "/strings/0x409/serialnumber", "THYME6");
    gw(g, "/strings/0x409/manufacturer", "thyme-mainline");
    gw(g, "/strings/0x409/product",      "Stage6 diag");
    gmkdir(g, "/configs/c.1");
    gw(g, "/configs/c.1/MaxPower", "500");
    gmkdir(g, "/configs/c.1/strings/0x409");
    gw(g, "/configs/c.1/strings/0x409/configuration", "diag");

    /* ACM 优先 */
    gmkdir(g, "/functions/acm.0");
    if (symlink("/sys/kernel/config/usb_gadget/g1/functions/acm.0",
                "/sys/kernel/config/usb_gadget/g1/configs/c.1/acm.0") == 0) {
        gw(g, "/UDC", udc);
        log_all("[init] gadget: ACM bound\n");
        return 1;
    }

    /* ECM 兜底 */
    gmkdir(g, "/functions/ecm.0");
    if (symlink("/sys/kernel/config/usb_gadget/g1/functions/ecm.0",
                "/sys/kernel/config/usb_gadget/g1/configs/c.1/ecm.0") == 0) {
        gw(g, "/UDC", udc);
        log_all("[init] gadget: ECM fallback bound\n");
        return 1;
    }

    log_all("[init] gadget: bind FAILED\n");
    return 0;
}

static void dump_cmdline(void)
{
    char buf[1024];
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0)
        return;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = '\0';
        log_all("[init] cmdline: ");
        log_all(buf);
        log_all("\n");
    }
}

int main(void)
{
    char first_udc[64] = {0};
    struct dirent *e;
    DIR *d;
    int got = 0;

    (void)mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755");
    (void)mkdir("/proc", 0555);
    (void)mkdir("/sys", 0755);
    (void)mount("proc", "/proc", "proc", 0, NULL);
    (void)mount("sysfs", "/sys", "sysfs", 0, NULL);
    (void)mkdir("/sys/kernel/config", 0755);
    (void)mount("configfs", "/sys/kernel/config", "configfs", 0, NULL);

    log_all("\n======== THYME_USERSPACE_REACHED ========\n");

    /* 枚举 UDC */
    d = opendir("/sys/class/udc");
    if (d) {
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.')
                continue;
            if (!got) {
                snprintf(first_udc, sizeof(first_udc), "%s", e->d_name);
                got = 1;
            }
            log_all("[init] udc: ");
            log_all(e->d_name);
            log_all("\n");
        }
        closedir(d);
    } else {
        log_all("[init] /sys/class/udc MISSING\n");
    }

    if (got)
        setup_gadget(first_udc);

    dump_cmdline();

    /* 保活：不自动退出 */
    for (;;)
        sleep(3600);

    return 0;
}