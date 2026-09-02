#!/usr/bin/env bash
#
# 将项目内的 thyme DTS 注入到 sm8250-mainline 内核源码树，
# 并在 qcom Makefile 里幂等地加入编译项。
#
# 用法：inject-thyme-dts.sh <kernel-tree-root>
#   $1  已 clone 的 linux-sm8250 源码树根目录
#
set -euo pipefail

KERNEL_ROOT="${1:?用法: inject-thyme-dts.sh <kernel-tree-root>}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DTS_SRC="${REPO_ROOT}/dts/sm8250-xiaomi-thyme.dts"
QCOM_DTS_DIR="${KERNEL_ROOT}/arch/arm64/boot/dts/qcom"
QCOM_MAKEFILE="${QCOM_DTS_DIR}/Makefile"
DTS_DST="${QCOM_DTS_DIR}/sm8250-xiaomi-thyme.dts"

[[ -f "${DTS_SRC}" ]] || { echo "找不到 DTS: ${DTS_SRC}"; exit 1; }
[[ -d "${QCOM_DTS_DIR}" ]] || { echo "非内核源码树或缺 qcom dts 目录: ${QCOM_DTS_DIR}"; exit 1; }

cp "${DTS_SRC}" "${DTS_DST}"
echo "已复制: ${DTS_DST}"

# 幂等追加 Makefile 条目（避免重复 CI 重复追加同一行）
LINE='dtb-$(CONFIG_ARCH_QCOM)	+= sm8250-xiaomi-thyme.dtb'
if grep -qF 'sm8250-xiaomi-thyme.dtb' "${QCOM_MAKEFILE}"; then
    echo "Makefile 已含 thyme 条目，跳过追加"
else
    printf '%s\n' "${LINE}" >> "${QCOM_MAKEFILE}"
    echo "已追加 Makefile: ${LINE}"
fi