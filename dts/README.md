# thyme DTS 移植（阶段 5）

Xiaomi Mi 10S (`thyme`, SM8250/Kona) 主线 Device Tree。

## 文件

- `sm8250-xiaomi-thyme.dts` — 第一版最小可启动骨架。

## 当前包含的子系统

| 子系统 | 状态 |
|---|---|
| SoC / model / compatible | ✅ `xiaomi,thyme` + `qcom,sm8250` |
| msm-id / board-id | ✅ `<356 0x20001>` / `<45 0>` |
| PM8150 / PM8150B / PM8150L / PM8009 | ✅ rpmh-regulators（复刻 elish 供电网） |
| chosen / stdout-path | ✅ `serial0 = &uart2`（se2，988000） |
| reserved-memory 差异 | ✅ xbl_aop/slpi/adsp/spss/cdsp_secure_heap（同 elish 布局） |
| UFS controller / PHY / regulator | ✅ |
| USB DWC3（usb_1 + typec + vbus） | ✅ USB 2.0 骨架 |
| 必要 pinctrl | ✅ tlmm 基础 + gpio-keys（vol-up = pm8150 gpio6） |

## 未移植（后续阶段）

Display / Touchscreen / Camera / 完整 Audio / UDFPS / 充电 / 传感器 / Wi-Fi / BT。

## 参考来源

- 主线模板：`sm8250-xiaomi-elish-common.dtsi`（同为 xiaomi SM8250）
- 厂商 MiCode：`thyme-sm8250-overlay.dts` / `thyme-sm8250.dtsi` / `xiaomi-sm8250-common.dtsi` / `thyme-pinctrl.dtsi`
- 实机反编译：`thyme-runtime.dts`（核对 msm-id / board-id / memory / reserved-memory / UFS / USB / serial）

## 构建（在 CI 内自动完成）

```bash
scripts/inject-thyme-dts.sh <linux-sm8250>
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs
```