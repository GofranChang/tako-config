# Tako ZMK — nice!nano OLED dongle

本分支 `codex/tako-dongle` 从稳定标签 `tako-analog-stable-2026-10-14`（`c262ce2`）创建。dongle 硬件与 [TOTEM feature/dongle_miryoku](https://github.com/GofranChang/zmk-config-totem/tree/feature/dongle_miryoku) 完全相同：nice!nano + I²C SH1106 128×64 OLED。

## 连接方式

```text
Tako 左半（BLE peripheral） ─┐
                           ├── nice!nano dongle（central）── USB ── 电脑
Tako 右半（BLE peripheral） ─┘
```

- dongle 是唯一主控，最多连接两半；Studio、Miryoku 层和按键处理在 dongle 上运行。主机蓝牙配置也属于 dongle。
- 左右半均为无线外围设备。把左半 USB 接电脑可以充电和校准，但键盘输入仍经 dongle；要恢复左半有线主控方案，请切回稳定标签并刷回两半固件。
- 两半保留 nice!view 数字电量、独立 USB 校准 Shell、原有 GPIO / MUX / ADC、50% 触发值、10/30 ms 扫描、矩阵断电 GPIO 停泊修复和 15 分钟深度睡眠。睡眠后按各半实体 reset 重启。
- 两半的空闲时间分别计算：只用一侧打字，另一侧仍可能在 15 分钟后深度睡眠。
- dongle 没有 EC 矩阵，使用 mock kscan；与两半共用同一份 34 键物理布局及矩阵映射，右侧保留 col-offset=5。USB 接通的 dongle 不进入深度睡眠。

## OLED 与依赖

OLED 配置逐项参照 TOTEM 的 `totem_dongle.overlay`：`pro_micro_i2c`（nice!nano SDA=P0.17、SCL=P0.20）、地址 0x3c、SH1106、128×64、segment-offset=2、multiplex-ratio=63、segment-remap、com-invdir、prechargep=0x22。电源接法沿用同款硬件。

dongle 使用 `englmaxi/zmk-dongle-display` 的 v0.3 兼容提交 `c6c4be11c5ede722086a2a1f355cec9c101b9390`，提供两半数字电量、层、连接、修饰键和猫动画等状态。电量来源编号按外围设备的配对槽位确定；首次配对先启动左半，再启动右半。

ZMK 保持 v0.3，因此 board 名仍为 `nice_nano_v2`，没有照搬新版 `nice_nano//zmk`。analog-matrix-module 固定 `3cb997880834b7147f96107d7564840f0013af1c`，保留已验证的功耗和闲置响应修复。参考仓库的 `zmk-nice-oled` 没有用于 dongle 构建，本分支不添加该无关依赖。

## 编译与固件

GitHub Actions 读取 `build.yaml`，生成左半、右半、dongle 和 settings_reset。远程构建前，须先将驱动仓库的固定提交推送到 GitHub，再推送本配置分支。

已配置的本地环境：

```sh
cd /Users/gofran/Documents/workspace/keyboard/zmk-local
./build-tako.sh all       # 两半 + dongle
./build-tako.sh both      # 仅两半
./build-tako.sh dongle    # 仅 dongle
./build-tako.sh reset     # 仅 settings_reset
```

本地 dongle 模式固件使用单独文件名，避免覆盖稳定版测试文件：`firmware/tako_left-dongle-mode.uf2`、`tako_right-dongle-mode.uf2`、`tako_dongle-dongle-mode.uf2`。两半只启用 calibrator snippet；dongle 只启用 Studio snippet，没有 EC 校准端口。

## 首次迁移与配对

**不要对两半直接刷 settings_reset：它会擦除 EC 校准数据。** 以下迁移没有更改 EC 校准存储路径。

1. 切换前分别用 USB 连接两半，运行 `ec KSCAN calibration export`，保存一份现有校准备份。
2. 编译临时解绑固件：`./build-tako.sh all --clear-bonds`。它使用 `CONFIG_ZMK_BLE_CLEAR_BONDS_ON_START=y`，只删除蓝牙绑定及 ZMK 的主机/外围地址记录，不清除 EC 校准数据；输出名带 `-clear-bonds.uf2`。本地固件目录已提供这三份临时固件。
3. 三个设备分别刷入对应的临时解绑固件，退出 UF2 引导模式后让它正常启动几秒。该临时固件每次启动都会清绑定，不能留作日常使用。
4. 再执行 `./build-tako.sh all`，给三个设备刷入对应的正常 `-dongle-mode.uf2` 固件。新固件仍可保留当前校准值，无需重做校准。
5. 关闭两半，dongle 接电脑；先启动左半并测试按键，再启动右半并测试。之后正常使用会自动重连。
6. 如果 dongle 原本刷的是另一把键盘且存在 Studio 改键数据，可以仅对 dongle 使用 settings_reset，再刷回 dongle 固件；它不存储两半的 EC 校准数据。settings_reset 是清空设置的固件，不是日常键盘固件。

之后仅更新固件通常无需再次清绑定。需要回到稳定版本时，切回标签后重编译两半；主从关系再次改变时也需要重新配对。

## 校准与数字电量

日常只将 dongle 插电脑。校准时需要 USB **直接连接待校准的半边**，dongle 不转发校准串口。两半各只有校准 Shell CDC；Studio 在 dongle 上。

```text
ec
ec KSCAN calibration start
ec KSCAN calibration save
ec KSCAN calibration export
ec KSCAN calibration load
```

开始校准后低值采样时全部松开；高值采样时逐颗按到底并保持，收到完成提示后松开，共 17 颗。完成后 save，重启读取并测试。Flash 路径仍为 `zmk/ec/cal/KSCAN`；坐标是 `(MUX 列索引, strobe 行索引)`，两组数组各 20 项。配套上位机选择 analog-matrix 协议。

nice!view 使用 ZMK 内置状态屏和 Montserrat 26 数字百分比。电量仍是电压估算，并非电流或精确容量测量。左右屏不显示 dongle 上的实时层状态；层状态请看 dongle OLED。

## 验证范围

本地已编译通过三个正常目标、三个临时解绑目标和 settings_reset。已核对最终 Kconfig：只有 dongle 为 central、连接两半且启用 Studio；两半为 peripheral、保留 USB 校准。通过 EDT 对比确认两半 EC / MUX 全部属性与稳定固件一致，三台的 34 键矩阵映射一致，右半 col-offset=5；dongle 为 mock kscan + SH1106 128×64，没有 EC 矩阵。正常版关闭启动清绑定，临时版打开。

尚未进行实机蓝牙配对、OLED 方向和休眠续航测试；本地编译通过不能代替这些验证。
