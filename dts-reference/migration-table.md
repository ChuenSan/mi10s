# thyme DTS 迁移表（4.19.157-perf → Linux 6.13）

Xiaomi Mi 10S（`thyme`，SM8250/Kona）Device Tree 迁移登记。

- **硬件事实来源**：`dts-reference/thyme-official-full.dts`（官方 4.19.157-perf 反编译合体树）
- **判定标准（唯一）**：`/Volumes/sgqMacOS/10s/reference/linux-sm8250-6.13`
- **组织方式参考**：6.13 `sm8250-xiaomi-elish-common.dtsi`
- **较新实现参考**：mi10 仓库 7.2-rc5 `sm8250-xiaomi-umi.dts`
- **目标产物**：`dts/sm8250-xiaomi-thyme.dts` → `make dtbs`/`dtbs_check` 通过

状态标记：✅已确认 · 🔧可迁移 · ⚠️需改写binding · ❌6.13缺失 · 🚧暂缓 · ⬜暂无driver

---

## 一、基础平台 / 启动

| 项 | 官方 4.19 thyme | 6.13 对应 | 迁移方式 | 状态 |
|---|---|---|---|---|
| model | `"Qualcomm Technologies, Inc. xiaomi thyme"` | `"Xiaomi Mi 10S"`（主线上习惯用品牌+机型） | 用友好名 | 🔧 |
| compatible | `qcom,kona-mtp`,`qcom,kona`,`qcom,mtp` | `xiaomi,thyme`,`qcom,sm8250` | 加 `xiaomi,thyme` 标识 | 🔧 |
| msms-id | `0x164 0x20001`（SM8250 v2.1） | `QCOM_ID_SM8250 0x20001` | 用 dt-bindings 宏 | ✅ |
| board-id | `0x2d 0x00`（45） | `<45 0>` | 直搬 | ✅ |
| chosen / stdout | BL 运行时填充 | `serial0 = &uart2`（se2） | 自定义 | 🔧 |
| memory | BL 运行时填充（静态是占位0） | 3 段布局，见下 | 需实测/或经 BL 填充 | ⚠️ |

> memory 静态占位 0，官方 `mem-offline` 节点给出 offline 三段 `<1@0x40000000, 0@0x40000000, 1@0xc0000000, 0@0x80000000, 2@0xc0000000, 1@0x40000000>` 提示真实 DDR 三段布局，但最终值待实机 `getvar`/BL 填充确认。

## 二、reserved-memory

| 区块 | 官方 4.19 thyme | 6.13 默认 | 迁移方式 | 状态 |
|---|---|---|---|---|
| hyp | `80000000` | `80000000` | 相同 | ✅ |
| xbl_aop | `80700000` | `80700000` | 相同 | ✅ |
| cmd_db | `80860000` | `80860000` | 相同 | ✅ |
| smem | `80900000` | `80900000` | 相同 | ✅ |
| removed | `80b00000` | `80b00000` | 相同 | ✅ |
| camera(pil) | `86200000` | `86200000` | 相同 | ✅ |
| wlan(pil) | `86700000` | `86700000` | 相同 | ✅ |
| ipa_fw/gsi | `86800000`/`86810000` | 同 | 相同 | ✅ |
| gpu(pil) | `8681a000` | `8681a000` | 相同 | ✅ |
| npu | `86900000` | `86900000` | 相同 | ✅ |
| video | `86e00000` | `86e00000` | 相同 | ✅ |
| cvp | `87300000` | `87300000` | 相同 | ✅ |
| cdsp | `87800000` | `87800000` | 相同 | ✅ |
| slpi | `88c00000` | `88c00000` | 相同 | ✅ |
| **adsp** | `8bb00000` | `8a100000` | **覆盖** `/delete-node` | ⚠️ |
| **spss** | `8e000000` | `8be00000` | **覆盖** `/delete-node` | ⚠️ |
| **cdsp_secure_heap** | `8e100000` | `8bf00000` | **覆盖** `/delete-node` | ⚠️ |
| cont_splash | `9c000000` | 缺 | 6.13 无 framebuffer enable 前暂缓 | 🚧 |
| disp_rdump/dfps | `9c000000`/`9e300000` | 缺 | 显示相关，随 DPU 补齐 | 🚧 |

> 结论：adsp/spss/cdsp_secure_heap 三块地址不同，需 `/delete-node/ &adsp_mem;` 等重定义。

## 三、PMIC（SPMI）

| PMIC | 官方 4.19 身份 | 6.13 dtsi | 迁移方式 | 状态 |
|---|---|---|---|---|
| PM8150 (SID 0) | SPMI 主 PMIC，RTC/GPIO/PON | `pm8150.dtsi` | 直 include | ✅ |
| PM8150B (SID 2) | 充电/bq/smb1390 | `pm8150b.dtsi` | 直 include | ✅ |
| PM8150L (SID 5) | 显示/背光/lcdb/amoled | `pm8150l.dtsi` | 直 include | ✅ |
| PM8009 (SID f) | 相机电源 | `pm8009.dtsi` | 直 include | ✅ |

> 官方 4.19 用 `qcom,spmi-pmic` 私有 + `qcom,qpnp-*` 驱动；6.13 已拆成 `pm8150.dtsi` 标准节点，直接 `#include`。

## 四、regulator（RPMh）

> **关键事实（本轮确认）**：官方 4.19 反编译树中「regulator 电压值」可精确读出（hex→μV），
> 但「supply 连接关系（phandle）」在反编译后失真为整数占位，无法直接读。
> 因此本节采用**双来源**：电压值 = 官方 4.19 事实；供电连接 = 6.13 elish/umi 参考 + 官方 4.19 节点名。

| Rail | 官方 4.19 电压 | 6.13 节点 label | 状态 |
|---|---|---|---|
| PM8150 S4A | 1800~1920 | `vreg_s4a_1p8: smps4` | ✅ 已写官方值 |
| PM8150 S5A | 1824~2040 | `vreg_s5a_1p9: smps5` | ✅ 已写官方值 |
| PM8150 S6A | 600~1128 | `vreg_s6a_0p95: smps6` | ✅ 已写官方值（下限修正） |
| PM8150 L2A | 3072 | `vreg_l2a_3p1: ldo2` | ✅ |
| PM8150 L3A | 928~932 | `vreg_l3a_0p9: ldo3` | ✅ |
| PM8150 L5A | 880 | `vreg_l5a_0p88: ldo5` | ✅ |
| PM8150 L6A | 1200 | `vreg_l6a_1p2: ldo6` | ✅ |
| PM8150 L7A | 1704~1800 | `vreg_l7a_1p8: ldo7` | ✅ 新增 |
| PM8150 L9A | 1200 | `vreg_l9a_1p2: ldo9` | ✅ |
| PM8150 L10A | 1800~2960 | `vreg_l10a_1p8: ldo10` | ✅ 新增 |
| PM8150 L12A | 1800 | `vreg_l12a_1p8: ldo12` | ✅ |
| PM8150 L13A | 3008 | `vreg_l13a_3p0: ldo13` | ✅ 新增 |
| PM8150 L14A | 1800~1880 | `vreg_l14a_1p88: ldo14` | ✅ 修正范围 |
| PM8150 L15A | 1800 | `vreg_l15a_1p8: ldo15` | ✅ 新增 |
| PM8150 L16A | 3024~3304 | `vreg_l16a_3p0: ldo16` | ✅ 新增 |
| PM8150 L17A | 2496~3008 | `vreg_l17a_3p0: ldo17` | ✅ 修正范围 |
| PM8150 L18A | 912~? | `vreg_l18a_0p9: ldo18` | ⚠️ min 值畸形，max 待确认 |
| PM8150L BOB | 3008~3960 | `vreg_bob: bob` | ✅ |
| PM8150L S8C | 1200~1400 | `vreg_s8c_1p35: smps8` | ✅ 修正范围 |
| PM8150L L1C~L11C | 见 4.19 | `vreg_l*c_*: ldo*` | ✅ L7C 上限畸形待修正，其余已写 |
| PM8009 S1F | 1200~1300 | `vreg_s1f_1p2: smps1` | ✅ 修正 |
| PM8009 S2F | 512~1100 | `vreg_s2f_0p5: smps2` | ✅ |
| PM8009 L1F~L7F | 见 4.19 | `vreg_l?f_*: ldo?` | ✅ 补全 7 个 LDO |

> 畸形待确认 rail（反编译负值/特殊编码失真）：`L18A`、`L7C`、`L3C` 等少数。
> **根因**：高通 4.19 对部分 LDO 用 `qcom,set = <0x03>`（多组）或负电压编码，
> `dtc -O dts` 反编译时把 4 字节负值误转成字符串字面量（如 `"", "\f5"`）。
> **处理**：mainline rpmh-regulator binding 无「负值/多 set」概念，均正电平。
> 这些 rail 以官方 min 值 + elish 合理 max 值替代，正式 build/实机前需复核真实需求。
> 本表中的「✅ 已写官方值」指正常 hex 值直接读出；「⚠️ 合理值替代」指畸形的少数。

## 五、UFS

| 项 | 官方 4.19 | 6.13 | 结论 |
|---|---|---|---|
| 控制器 | `ufshc@1d84000` | `ufs_mem_hc`（`qcom,sm8250-ufshc`） | ✅ 平台级内置 |
| phy | `ufsphy_mem` | `ufs_mem_phy`（`qcom,sm8250-qmp-ufs-phy`） | ✅ |
| 供电 | vcc/vccq/vccq2 三轨 | `vreg_l17a_3p0`/`vreg_l6a_1p2`/`vreg_s4a_1p8` | ✅ 用 6.13 elish 三轨写法 |
| reset | 官方用 pinctrl（ufs_dev_reset） | mainline 用 `resets`（内置），**无 reset-gpios** | ✅ 已删错误 reset-gpios |

## 六、USB

| 项 | 官方 4.19 thyme | 6.13 | 结论 |
|---|---|---|---|
| usb_1 | `ssusb@a600000`，dwc3 `high-speed` | `&usb_1`+`&usb_1_dwc3` | ✅ `otg` + `maximum-speed=high-speed`，删 USB3 phy |
| usb_1 qmpphy | combo DP phy（供 DP 输出） | `&usb_1_qmpphy` | ✅ 保留供 DP |
| usb_2 | `ssusb@a800000`，dwc3 `super-speed` | `&usb_2`+`&usb_2_dwc3` | ✅ `host` + `maximum-speed=super-speed` |
| usb_2 qmpphy | usb3 phy | `&usb_2_qmpphy` | ✅ 新增引用 |

> **与 elish 差异**：elish 是 USB2.0 only（两者都 high-speed）；thyme usb_2 是 super-speed。

## 七、GPIO / pinctrl（TLMM）

| 项 | 官方 4.19 | 6.13 | 状态 |
|---|---|---|---|
| tlmm | `pinctrl@f000000` | `tlmm`（平台级内置） | ✅ |
| gpio-keys vol-up | pm8150 gpio6 | `pm8150_gpios 6` + `pon_resin` | ✅ |
| 屏/触摸/指纹 GPIO | 官方 pmx_* 私有 | 待翻译 | 🚧 后续批次 |

---

## 八、PCIe

| 项 | 官方 4.19 thyme | 6.13 | 状态 |
|---|---|---|---|
| 控制器 | `pcie0@1c00000`（Wi-Fi 挂这） | `&pcie0`（`qcom,sm8250-pcie`） | ✅ |
| PHY | pcie phy | `&pcie0_phy`（`qcom,sm8250-qmp-gen3x2-pcie-phy`） | ✅ 供电 vdda-phy=l5a/vdda-pll=l9a |
| endPoint | `cnss_pci`（cnss 私有） | `&pcieport0 { wifi@0 { pci17cb,1101 } }` | ✅ 去 cnss 私有 |
| pcie1/pcie2 | 存在（modem/其他） | 暂不迁移（非 Wi-Fi 路径） | 🚧 |

## 九、QCA6390 Wi-Fi

| 项 | 官方 4.19 | 6.13 elish 参考 | 状态 |
|---|---|---|---|
| 设备 | `qcom,cnss-qca6390@b0000000`（`qcom,cnss-qca6390`） | `qca6390-pmu`（`qcom,qca6390-pmu`）+ `wifi@0`（`pci17cb,1101`） | ✅ 换 mainline binding |
| enable GPIO | `wlan-en-gpio=<tlmm 20>` | `wlan-enable-gpios=<&tlmm 20>` | ✅ GPIO 一致 |
| 供电(五路) | aon/dig/io/rfa1/rfa2 = 950/950/1800/1900/1350mV | pmu 内部 ldo0~ldo9 + 外部 s6a/s5a/s4a/s8c | ✅ 电压值吻合 |
| **calibration-variant** | BSP 无此属性 | elish=`"Xiaomi_Pad_5Pro"` | ⚠️ **thyme 值待从实机 firmware 确认** |

> 关键：QCA6390 供电网 thyme 与 elish 电压一致（aon=950/dig=950/io=1800/rfa1=1900/rfa2=1350，
> asd=3024~3304→L16A 完全匹配），可复用 elish 的 pmu ldo 结构。

## 十、Bluetooth

| 项 | 官方 4.19 thyme | 6.13 elish 参考 | 状态 |
|---|---|---|---|
| 数据链路 | SLIMbus（`qca6390` codec）+ cnss 私有 | `&uart6 { bluetooth { qcom,qca6390-bt } }` | ✅ UART 取代 SLIMbus |
| **UART** | `qupv3_se6_4uart@998000`（`wakeup-byte=0xfd`） | `&uart6`（`serial@998000`） | ✅ 同 se6，thyme BT 用 uart6 |
| enable/reset GPIO | `bt-reset-gpio=21`、`bt-sw-ctrl-gpio=124` | `bt-enable-gpios=<&tlmm 21>` | ✅ GPIO 一致 |
| 供电 | 五路与 Wi-Fi 共享 | pmu 内部 ldo（btcmx/aon/rfa*） | ✅ |

> **与 elish 差异**：thyme debug 串口=`uart2`，BT=`uart6`（两者独立）；elish debug 与 BT 都挤 uart6。
> 官方 4.19 的 `qca,bt-reset-gpio=21` + `qca,bt-sw-ctrl-gpio=124` 在 mainline `qcom,qca6390-bt`
> binding 里由 `enable-gpios` 表达，无需逐字照搬 BSP 私有 `qca,*` 属性。

## 十一、GPU / GMU

| 项 | 官方 4.19 | 6.13 | 状态 |
|---|---|---|---|
| GPU | `qcom,kgsl-3d0@3d00000`（kgsl 私有） | `&gpu`（`qcom,adreno-650`）+ `&gmu` | ✅ 平台级内建 |
| firmware | `a650_zap` | `zap-shader` 子节点 | ✅ thyme 无特殊差异 |

> **结论**：thyme GPU 无特有差异（Adreno 650，平台级 sm8250.dtsi 已有），仅需 enable。

## 十二、DPU / MDSS / DSI

| 项 | 官方 4.19 | 6.13 | 状态 |
|---|---|---|---|
| MDSS | `mdss_mdp@ae00000` | `&mdss`+`&mdss_mdp`（`qcom,sm8250-dpu`） | ✅ 平台级内建 |
| DSI0 | `mdss_dsi_ctrl0@ae94000` | `&mdss_dsi0`（`qcom,mdss-dsi-ctrl`） | ✅ |
| DSI0 PHY | `mdss_dsi_pll@ae94900` | `&mdss_dsi0_phy`（`qcom,sm8250-dsi-phy-14nm`） | ✅ |
| panel | `qcom,mdss_dsi_*`（BSP） | `&mdss_dsi0 { panel@0 { drm_panel } }` | ⚠️ 见下 |

## 十三、thyme Panel

| 项 | 官方 4.19 thyme 事实 | mainline | 状态 |
|---|---|---|---|
| 真机屏型号 | `xiaomi 42 04 0a` / `xiaomi 42 02 0b`（cnss 42 族） | `csot,j2-mp-42-02-0b-dsc` | 需 backport |
| 分辨率 | 1080×2340（`0x438 × 0x924`） | driver mode 1080×2340 | ✅ 匹配 |
| 模式 | cmd mode + DSC（`compression-mode=dsc`） | DSC（drm_dsc） | ✅ |
| 物理尺寸 | 6.67" | 71.0×153.7mm | ✅ 10S 6.67" |
| vddio | — | `vreg_l14a_1p8` | ✅ |
| reset/te gpio | BSP pinctrl | `reset-gpios=<tlmm 12>`,`disprate-gpios=<tlmm 50>` | ✅ 参考 umi |

> **关键判定**：thyme 真屏 = CSOT `csot,j2-mp-42-02-0b-dsc`（1080×2340+DSC，6.67"）。
> **该 driver 在 7.2（umi）里存在，但 6.13 缺失** → **需要 backport**
> （panel-j2-mp-42-02-0b-dsc.c + binding yaml + Kconfig/Makefile）。
> 6.13 里的 `boe,bf060y8m-aj0`（1080×2160）分辨率不符，**不能硬套**。

## 待办 / 未迁移（后续批次）

- **下一批**：Audio、Touchscreen、Charging、Sensors、UDFPS 指纹
- **Panel driver backport**：`csot,j2-mp-42-02-0b-dsc`（7.2→6.13，需新增 driver）
- Camera：暂时后置，不作阻塞项

## Vendor 私有 property 待筛清单（重点）

官方 4.19 中大量 `qcom,*`/`mi,*`/`xiaomi,*` 私有属性，逐项判：硬件事实 vs 旧 BSP vs 已有 mainline 替代。首批已识别：

- `qcom,kona-*`、`qcom,gdsc`、`qcom,devbw`、`qcom,bimc-bwmon*`、`qcom,mem-lat` → 6.13 平台级已内建，**无需搬**
- `qcom,spmi-pmic-arb`/`spmi-pmic`/`qpnp-*` → 6.13 已拆 `pm8150.dtsi`，**无需搬**
- `xiaomi-touch`、`xiaomi,usbpd-pm`、`xiaomi,cp-qc30` → 6.13 无对应，**待筛**（第三批）