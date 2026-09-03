// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 第一次 Runtime bring-up 诊断 init（ARM64 静态，无 busybox）。
 *
 * 目标：确立「Kernel → DTB → initramfs → /init → userspace」链路，
 * 建立 USB gadget（ACM）作为唯一可观察通道，并输出分阶段诊断。
 *
 * 阶段标记：
 *   THYME_STAGE_1_INIT          devtmpfs/proc/sysfs/configfs 挂载完成
 *   THYME_STAGE_2_SYSFS         /sys/class/udc 枚举完成
 *   THYME_STAGE_3_USB_GADGET    USB ACM gadget 绑定完成
 *   THYME_USERSPACE_REACHED     进入保活循环前（kernel+initramfs+userspace 全通）
 *   THYME_DRM_STATE             DRM/面板状态 dump
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>

static int acmfd = -1;      /* /dev/ttyGS0 句柄，-1 表示不可用 */

static void w(const char *p, const char *s)
{
    int fd = open(p, O_WRONLY);
    if (fd < 0)
        return;
    (void)write(fd, s, strlen(s));
    close(fd);
}

/* 写所有可观察通道：kmsg + console + ACM（若已开） */
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
    if (acmfd >= 0)
        (void)write(acmfd, s, strlen(s));
}

static void logf(const char *s)
{
    log_all(s);
    log_all("\n");
}

static void stamp(const char *tag)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", tag);
    logf(buf);
}

/* 写 gadget 配置值：base + sub 拼接 */
static void gw(const char *base, const char *sub, const char *val)
{
    char path[256];
    snprintf(path, sizeof(path), "%s%s", base, sub);
    w(path, val);
}

/* 建目录：base + sub 拼接 */
static void gmkdir(const char *base, const char *sub)
{
    char path[256];
    snprintf(path, sizeof(path), "%s%s", base, sub);
    (void)mkdir(path, 0755);
}

/* 建最简 USB gadget：ACM。返回 1 绑定成功 */
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

    /* ACM */
    gmkdir(g, "/functions/acm.0");
    if (symlink("/sys/kernel/config/usb_gadget/g1/functions/acm.0",
                "/sys/kernel/config/usb_gadget/g1/configs/c.1/acm.0") == 0) {
        gw(g, "/UDC", udc);
        log_all("[init] gadget: ACM bound to ");
        log_all(udc);
        log_all("\n");
        return 1;
    }

    log_all("[init] gadget: ACM bind FAILED\n");
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

/* 递归读 /sys/class/drm/<dev>/ 下的 status/enabled 等只读文本 */
static void dump_drm(void)
{
    DIR *d = opendir("/sys/class/drm");
    struct dirent *e;
    if (!d) {
        logf("[init] /sys/class/drm MISSING");
        return;
    }
    while ((e = readdir(d))) {
        char path[256], buf[256];
        int fd;
        ssize_t n;
        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "/sys/class/drm/%s/status", e->d_name);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            snprintf(path, sizeof(path), "/sys/class/drm/%s/dpms", e->d_name);
            fd = open(path, O_RDONLY);
        }
        if (fd < 0)
            continue;
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            log_all("[init] drm ");
            log_all(e->d_name);
            log_all(": ");
            log_all(buf);
            log_all("\n");
        }
    }
    closedir(d);
}

/* 枚举 /dev 关键设备，取舍那些能佐证 backing 链路的 */
static void dump_dev(void)
{
    static const char *want[] = {
        "ttyGS0", "ttyMSM0", "udc", "kmsg", "console", NULL
    };
    struct stat st;
    int i;

    for (i = 0; want[i]; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/%s", want[i]);
        if (stat(path, &st) == 0)
            logf(path);   /* 每行一个关键设备 */
    }
}

int main(void)
{
    struct dirent *e;
    DIR *d;
    char first_udc[64] = {0};
    int got = 0;

    (void)mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755");
    (void)mkdir("/proc", 0555);
    (void)mkdir("/sys", 0755);
    (void)mount("proc", "/proc", "proc", 0, NULL);
    (void)mount("sysfs", "/sys", "sysfs", 0, NULL);
    (void)mkdir("/sys/kernel/config", 0755);
    (void)mount("configfs", "/sys/kernel/config", "configfs", 0, NULL);

    stamp("THYME_STAGE_1_INIT");

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

    stamp("THYME_STAGE_2_SYSFS");

    /* 建立 ACM gadget，随后 /dev/ttyGS0 出现 */
    if (got)
        setup_gadget(first_udc);

    /* 打开 ACM 串口，此后 log_all 也写 ttyGS0 */
    acmfd = open("/dev/ttyGS0", O_WRONLY);
    if (acmfd < 0)
        acmfd = open("/dev/ttyGS0", O_RDWR);

    stamp("THYME_STAGE_3_USB_GADGET");

    dump_cmdline();
    dump_drm();
    dump_dev();

    stamp("THYME_DRM_STATE");
    stamp("THYME_USERSPACE_REACHED");

    /* 保活：不自动退出 */
    for (;;)
        sleep(3600);

    return 0;
}