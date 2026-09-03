#!/usr/bin/env bash
# 生成阶段 6 最小 bring-up initramfs（ARM64，静态 init，无 busybox 依赖）。
#
# 用法：make-initramfs.sh <outdir>
# 产物：<outdir>/initramfs.cpio.gz
#
# 依赖：aarch64-linux-gnu-gcc、cpio、gzip；
# 交叉工具链由 CI 已安装，本机无交叉工具链时需自行提供。
set -euo pipefail

OUTDIR="${1:?usage: make-initramfs.sh <outdir>}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR/rootfs"

# 1. 静态编译最小 init
"${CROSS_COMPILE}gcc" -static -Os -s \
    -o "$OUTDIR/rootfs/init" "$SRC_DIR/init.c"

# 2. 必要目录与入口符号链接（内核约定 /init 为第 1 个进程）
mkdir -p "$OUTDIR/rootfs/dev" \
         "$OUTDIR/rootfs/proc" \
         "$OUTDIR/rootfs/sys" \
         "$OUTDIR/rootfs/tmp"
chmod 1777 "$OUTDIR/rootfs/tmp"

# 3. 打包成 gzip 的 newc cpio
(
    cd "$OUTDIR/rootfs"
    find . -print0 | cpio --null -o -H newc --quiet | gzip -9
) > "$OUTDIR/initramfs.cpio.gz"

echo "initramfs: $OUTDIR/initramfs.cpio.gz ($(stat -c%s "$OUTDIR/initramfs.cpio.gz") bytes)"