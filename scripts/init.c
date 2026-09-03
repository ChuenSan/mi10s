// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 阶段 6 诊断 init v3（ARM64，静态，无 busybox）。
 *
 * 目标：确立「Kernel → initramfs → /init → userspace 已到达」这一事实，
 * 以可观察的「约 30s 后主动 reboot」作为判断信号。
 *
 * 行为：
 *   1. mount devtmpfs/proc/sysfs/configfs
 *   2. 向 /dev/kmsg 写 THYME_STAGE6_USERSPACE_REACHED
 *   3. 枚举 /sys/class/udc；若有 UDC，建最简 USB gadget（ACM 优先，ECM 兜底）
 *   4. 等约 30s 主动 reboot(RB_AUTOBOOT)
 *
 * 判断：
 *   B 槽每次约 30s 后稳定自动重启 → Kernel 已启动 + initramfs 已加载 + /init 已执行
 *   Mac 同时枚举到 USB 设备 → USB/DWC3 已工作
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
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
    int fd;
    fd = open("/dev/kmsg", O_WRONLY);
    if (fd >= 0) { (void)write(fd, s, strlen(s)); close(fd); }
    fd = open("/dev/console", O_WRONLY);
    if (fd >= 0) { (void)write(fd, s, strlen(s)); close(fd); }
}

/* 建最简 USB gadget：ACM 优先，失败回落 ECM。返回 1 绑定成功 */
static int setup_gadget(const char *udc)
{
    const char *g = "/sys/kernel/config/usb_gadget/g1";

    (void)mkdir(g, 0755);
    w(g "/idVendor",  "0x18d1");
    w(g "/idProduct", "0x4d11");
    w(g "/bcdDevice", "0x0100");
    w(g "/bcdUSB",    "0x0200");
    (void)mkdir(g "/strings/0x409", 0755);
    w(g "/strings/0x409/serialnumber", "THYME6");
    w(g "/strings/0x409/manufacturer", "thyme-mainline");
    w(g "/strings/0x409/product",      "Stage6 diag");
    (void)mkdir(g "/configs/c.1", 0755);
    w(g "/configs/c.1/MaxPower", "500");
    (void)mkdir(g "/configs/c.1/strings/0x409", 0755);
    w(g "/configs/c.1/strings/0x409/configuration", "diag");

    /* ACM 优先 */
    (void)mkdir(g "/functions/acm.0", 0755);
    if (symlink(g "/functions/acm.0", g "/configs/c.1/acm.0") == 0) {
        w(g "/UDC", udc);
        log_all("[init] gadget: ACM bound\n");
        return 1;
    }

    /* ECM 兜底 */
    (void)mkdir(g "/functions/ecm.0", 0755);
    if (symlink(g "/functions/ecm.0", g "/configs/c.1/ecm.0") == 0) {
        w(g "/UDC", udc);
        log_all("[init] gadget: ECM fallback bound\n");
        return 1;
    }

    log_all("[init] gadget: bind FAILED\n");
    return 0;
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

    log_all("\n======== THYME_STAGE6_USERSPACE_REACHED ========\n");

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

    log_all("[init] will auto-reboot in 30s\n");
    sleep(30);

    sync();
    reboot(RB_AUTOBOOT);
    return 0;
}