# Panel driver backport：csot,j2-mp-42-02-0b-dsc

Xiaomi Mi 10S (thyme) / Mi 10 (umi) 主屏 CSOT J2 MP 42 02 0B 面板驱动
backport（7.2 → 6.13）。

## 背景

- thyme 真机面板：CSOT `csot,j2-mp-42-02-0b-dsc`，1080×2340，cmd mode + DSC 1.1，6.67"
- Linux 6.13 **无**对应 driver；Mi 10 (umi) 7.2-rc5 有 `panel-j2-mp-42-02-0b-dsc.c`
- 上游 mainline / linux-next / lore 均无此 panel 的正式提交（确认于 backport 阶段）

## 来源与职责

| 来源 | 提供内容 |
|---|---|
| 官方 4.19 thyme DTS | 硬件事实：分辨率/刷新/mode/DSC 参数/refresh gpio(GPIO50)/reset |
| vendor `dsi-panel-j2-mp-42-02-0b-dsc-cmd.dtsi` | DCS init sequence（由 generator 转成 DCS write_seq） |
| Mi 10 7.2 driver | init sequence / mode 参数（probe 已损坏，弃用其 probe） |
| Linux 6.13 `panel-lg-sw43408.c` | probe/callback/DSC 挂载结构模板 |

## 7.2 半成品的问题（本 backport 修复）

1. `probe` 用了 6.14+ 才有、6.13 没有的 `devm_drm_panel_alloc()` → 改为 `devm_kzalloc` + `drm_panel_init` + `drm_panel_add`
2. `of_device_id` 缺 `.compatible` 字段
3. compatible 串掉字：`csot,j2-mp-42-02-0b-dsc`（少 "2"）→ 修正
4. `drm_panel_add` / `mipi_dsi_attach` 顺序错误 → 修正为 add 先、attach 后
5. regulator 获取缺失 → 补 `devm_regulator_get(dev, "vddio")`

## 保留未改（vendor 事实）

- `j2_mp_42_02_0b_dsc_on()` 完整 DCS init sequence
- `j2_mp_42_02_0b_dsc_mode`：1080×2340，width_mm 710 / height_mm 1537（6.67"）
- DSC 配置：version 1.1，slice_width 540 / slice_height 12 / slice_count 2，
  bits_per_component 8，bits_per_pixel 8<<4，block_pred_enable（数学自洽）

## 文件

- `patches/panel-j2-mp-42-02-0b-dsc.c` — driver
- `patches/csot,j2-mp-42-02-0b-dsc.yaml` — DT binding
- `scripts/inject-panel-backport.sh` — CI 注入脚本
- `.github/workflows/build-integration.yml` — integration CI

## 待实机验证项（编译不阻塞）

- reset / disprate GPIO 极性与时序
- vddio 电压 rail 精确确认（当前 vreg_l14a_1p88）
- DSC slice 参数实机点亮核对