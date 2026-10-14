# Tako ZMK — dongle 与左侧主设备双模式

本分支 `codex/tako-dongle` 从稳定标签 `tako-analog-stable-2026-10-14`（`c262ce2`）创建。dongle 硬件与 [TOTEM feature/dongle_miryoku](https://github.com/GofranChang/zmk-config-totem/tree/feature/dongle_miryoku) 完全相同：nice!nano + I²C SH1106 128×64 OLED。

## 六个编译产物

`build.yaml` 一次固定生成以下六个 UF2；文件名显式区分两套连接方式。

| 文件名 | 用途 |
| --- | --- |
| `settings_reset.uf2` | 清空 nice!nano 设置；不是日常固件 |
| `tako_dongle.uf2` | nice!nano + SH1106 dongle 主设备，连接两半 |
| `tako_left_dongle.uf2` | dongle 模式左侧从设备 |
| `tako_right_dongle.uf2` | dongle 模式右侧从设备 |
| `tako_left_central.uf2` | 左侧 USB 连电脑的主设备，连接右半 |
| `tako_right_peripheral.uf2` | 左主右从模式的右侧从设备 |

两份右侧固件的配置和功能相同，分别命名便于成套选择。连接哪个主设备由蓝牙绑定决定，文件名本身不会指定绑定对象。左侧角色由构建项显式选择：默认从设备；左主构建通过 CMake 覆盖为 central，并开启 Studio。

## dongle 模式连接方式

```text
Tako 左半（BLE peripheral） ─┐
                           ├── nice!nano dongle（central）── USB ── 电脑
Tako 右半（BLE peripheral） ─┘
```

- dongle 是唯一主控，最多连接两半；Studio、Miryoku 层和按键处理在 dongle 上运行。主机蓝牙配置也属于 dongle。
- 此模式左右半均为无线外围设备。把左半 USB 接电脑可以充电和校准，但键盘输入仍经 dongle。左主右从模式直接使用同分支的另一组固件，见下文。
- 两半保留 nice!view 数字电量、独立 USB 校准 Shell、原有 GPIO / MUX / ADC、50% 触发值、10/30 ms 扫描、矩阵断电 GPIO 停泊修复和 15 分钟深度睡眠。睡眠后按各半实体 reset 重启。
- 两半的空闲时间分别计算：只用一侧打字，另一侧仍可能在 15 分钟后深度睡眠。
- dongle 没有 EC 矩阵，使用 mock kscan；与两半共用同一份 34 键物理布局及矩阵映射，右侧保留 col-offset=5。USB 接通的 dongle 不进入深度睡眠。

## 左主右从模式

```text
Tako 右半（BLE peripheral）── Tako 左半（central）── USB ── 电脑
```

刷 `tako_left_central.uf2` + `tako_right_peripheral.uf2`。左半保留 Studio 和 EC 校准两个 CDC 端口；右半保留 EC 校准端口。左主最多连接一个右半；dongle 不参与此模式，请断开 dongle。

在两种拓扑间切换不是自动切主：需给左半刷对应角色固件，并清理两端旧蓝牙绑定后重新配对；右半只更新固件不会清除旧绑定。

## OLED 与依赖

OLED 配置逐项参照 TOTEM 的 `totem_dongle.overlay`：`pro_micro_i2c`（nice!nano SDA=P0.17、SCL=P0.20）、地址 0x3c、SH1106、128×64、segment-offset=2、multiplex-ratio=63、segment-remap、com-invdir、prechargep=0x22。电源接法沿用同款硬件。

dongle 使用 `englmaxi/zmk-dongle-display` 的 v0.3 兼容提交 `c6c4be11c5ede722086a2a1f355cec9c101b9390`，提供两半数字电量、层、连接、修饰键和猫动画等状态。电量来源编号按外围设备的配对槽位确定；首次配对先启动左半，再启动右半。

ZMK 保持 v0.3，因此 board 名仍为 `nice_nano_v2`，没有照搬新版 `nice_nano//zmk`。analog-matrix-module 固定 `3cb997880834b7147f96107d7564840f0013af1c`，保留已验证的功耗和闲置响应修复。参考仓库的 `zmk-nice-oled` 没有用于 dongle 构建，本分支不添加该无关依赖。

## 编译与固件

GitHub Actions 读取 `build.yaml`，一次生成上述六个固件，使用独立 artifact-name，两个右侧构建不会互相覆盖。远程构建前，须先将驱动仓库的固定提交推送到 GitHub，再推送本配置分支。

已配置的本地环境：

```sh
cd /Users/gofran/Documents/workspace/keyboard/zmk-local
./build-tako.sh all          # 一次构建六个固件（默认）
./build-tako.sh dongle-set   # dongle + 左右从设备，共三个
./build-tako.sh wired-set    # 左主 + 右从，共两个
./build-tako.sh both         # dongle 模式的左右从设备
./build-tako.sh left-central # 仅左主
./build-tako.sh dongle       # 仅 dongle
./build-tako.sh reset        # 仅 settings_reset
```

本地输出与远程 artifact-name 相同，位于 `zmk-local/firmware/`。仅左主和 dongle 启用 Studio snippet；所有实际键盘半边均保留 calibrator snippet。

## 首次迁移与配对

**不要对两半直接刷 settings_reset：它会擦除 EC 校准数据。** 以下迁移没有更改 EC 校准存储路径。

1. 切换前分别用 USB 连接两半，运行 `ec KSCAN calibration export`，保存一份现有校准备份。
2. 编译临时解绑固件：`./build-tako.sh dongle-set --clear-bonds`。它使用 `CONFIG_ZMK_BLE_CLEAR_BONDS_ON_START=y`，只删除蓝牙绑定及 ZMK 的主机/外围地址记录，不清除 EC 校准数据；输出名带 `-clear-bonds.uf2`。
3. 三个设备分别刷入对应的临时解绑固件，退出 UF2 引导模式后让它正常启动几秒。该临时固件每次启动都会清绑定，不能留作日常使用。
4. 再执行 `./build-tako.sh dongle-set`，给三个设备刷入表中对应的正常固件（不带 `-clear-bonds`）。新固件仍可保留当前校准值，无需重做校准。
5. 关闭两半，dongle 接电脑；先启动左半并测试按键，再启动右半并测试。之后正常使用会自动重连。
6. 如果 dongle 原本刷的是另一把键盘且存在 Studio 改键数据，可以仅对 dongle 使用 settings_reset，再刷回 dongle 固件；它不存储两半的 EC 校准数据。settings_reset 是清空设置的固件，不是日常键盘固件。

之后仅更新固件通常无需再次清绑定。需要切换为左主右从时，先用 `./build-tako.sh wired-set --clear-bonds` 编译并给两半刷入临时解绑固件，启动后再用 `./build-tako.sh wired-set` 的正常左主 / 右从固件覆盖；不要让临时解绑版长期运行。

## 校准与数字电量

dongle 模式日常只将 dongle 插电脑。校准时需要 USB **直接连接待校准的半边**，dongle 不转发校准串口。dongle 模式两半各只有校准 Shell CDC；Studio 在 dongle 上。左主右从模式则在左半保留 Studio 和校准两个 CDC，选择有 Shell 提示符的端口。

```text
ec
ec KSCAN calibration start
ec KSCAN calibration save
ec KSCAN calibration export
ec KSCAN calibration load
```

开始校准后低值采样时全部松开；高值采样时逐颗按到底并保持，收到完成提示后松开，共 17 颗。完成后 save，重启读取并测试。Flash 路径仍为 `zmk/ec/cal/KSCAN`；坐标是 `(MUX 列索引, strobe 行索引)`，两组数组各 20 项。配套上位机选择 analog-matrix 协议。

nice!view 使用 ZMK 内置状态屏和 Montserrat 26 数字百分比。电量仍是电压估算，并非电流或精确容量测量。dongle 模式左右屏不显示 dongle 上的实时层状态；层状态请看 dongle OLED。左主右从模式的层状态在左主设备屏幕显示。

## 验证范围

六个目标已全部本地编译通过，并核对最终角色分别为：dongle central（连接两半）、左主 central（连接一半）、其余半边 peripheral。两套主设备都启用 Studio，四个半边构建都保留 USB 校准，正常版不在启动时清绑定。两份右侧 UF2 的 SHA-256 相同，二进制完全一致。

已通过 EDT 对比确认 dongle 模式两半 EC / MUX 全部属性与稳定固件一致，三台的 34 键矩阵映射一致，右半 col-offset=5；dongle 为 mock kscan + SH1106 128×64，没有 EC 矩阵。

尚未进行实机蓝牙配对、OLED 方向和休眠续航测试；本地编译通过不能代替这些验证。
