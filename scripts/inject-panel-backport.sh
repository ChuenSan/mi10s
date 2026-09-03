#!/usr/bin/env bash
#
# 将 backport 的 CSOT j2-mp-42-02-0b-dsc panel driver + binding 注入到
# sm8250 内核源码树，并幂等地在 Kconfig / Makefile 里加入条目。
#
# 用法：inject-panel-backport.sh <kernel-tree-root>
#
set -euo pipefail

KERNEL_ROOT="${1:?用法: inject-panel-backport.sh <kernel-tree-root>}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PANEL_DIR="${KERNEL_ROOT}/drivers/gpu/drm/panel"
BINDING_DIR="${KERNEL_ROOT}/Documentation/devicetree/bindings/display/panel"

DRV_SRC="${SCRIPT_DIR}/../patches/panel-j2-mp-42-02-0b-dsc.c"
YAML_SRC="${SCRIPT_DIR}/../patches/csot,j2-mp-42-02-0b-dsc.yaml"

[[ -f "${DRV_SRC}" ]] || { echo "找不到 driver: ${DRV_SRC}"; exit 1; }
[[ -f "${YAML_SRC}" ]] || { echo "找不到 binding: ${YAML_SRC}"; exit 1; }

cp "${DRV_SRC}"  "${PANEL_DIR}/panel-j2-mp-42-02-0b-dsc.c"
cp "${YAML_SRC}" "${BINDING_DIR}/csot,j2-mp-42-02-0b-dsc.yaml"
echo "已复制 driver + binding"

# Kconfig 幂等追加 config 条目
KCONFIG="${PANEL_DIR}/Kconfig"
if grep -qF 'DRM_PANEL_CSOT_MP42020B' "${KCONFIG}"; then
    echo "Kconfig 已含 config 条目，跳过"
else
    cat >> "${KCONFIG}" << 'EOF'

config DRM_PANEL_CSOT_MP42020B
	tristate "CSOT J2 MP 42 02 0B DSC AMOLED panel"
	depends on OF
	depends on DRM_MIPI_DSI
	depends on BACKLIGHT_CLASS_DEVICE
	help
	  Say Y here to enable support for the CSOT J2 MP 42 02 0B
	  6.67" 1080x2340 AMOLED command mode panel with DSC 1.1
	  compression, used on Xiaomi Mi 10S (thyme) and Mi 10 (umi).
EOF
    echo "已追加 Kconfig 条目"
fi

# Makefile 幂等追加 entry
MAKEFILE="${PANEL_DIR}/Makefile"
if grep -qF 'panel-j2-mp-42-02-0b-dsc.o' "${MAKEFILE}"; then
    echo "Makefile 已含 entry，跳过"
else
    printf 'obj-$(CONFIG_DRM_PANEL_CSOT_MP42020B) += panel-j2-mp-42-02-0b-dsc.o\n' >> "${MAKEFILE}"
    echo "已追加 Makefile entry"
fi

grep -n 'MP42020B\|j2-mp-42-02' "${KCONFIG}" "${MAKEFILE}"