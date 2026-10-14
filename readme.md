# Tako ZMK — 双半独立 2.4G dongle 实验版

分支：`codex/tako-2g4-dongle`，基于原 BLE dongle / 左主双模式版本。新增的分体传输来自本地 `zmks` 的 `codex/2g4-split`，基于其 `2g4-impl` 分支开发。**代码、协议测试和编译已完成，尚未做射频实机验证；目前作为实验固件使用。**

```text
Tako 左半（ESB slot 0） ──2.4G──┐
                               ├── nice!nano + SH1106 dongle ──USB── 电脑
Tako 右半（ESB slot 1） ──2.4G──┘
```

左右半各自连接 dongle，dongle 统一处理 Miryoku、组合键和 USB Studio。三台设备都关闭 Bluetooth，使用专门的 ESB 分体协议；没有“先连接左半再由左半转发”的链路。

## Keymap

<!-- keymap:start -->

![Tako Base 层](keymap-drawer/tako-base.svg)

主字为点按，下方为长按；表格中的 `/` 分隔点按与长按。`2x` 表示双击切层；右上角标注为 Shift 功能。组合键框连接的按键需要同时按下。

| 左1 | 左2 | 左3 | 左4 | 左5 | 右1 | 右2 | 右3 | 右4 | 右5 |
|---|---|---|---|---|---|---|---|---|---|
| Q | W | E | R | T | Y | U | I | O | P |
| A / GUI | S / Alt | D / Ctrl | F / Shift | G | H | J / Shift | K / Ctrl | L / Alt | &#x27; / GUI |
| Z / Button | X / AltGr | C | V | B | N | M | , | . / AltGr | / / Button |

拇指键（左至右）：SPACE / Nav；TAB / Mouse；RET / Sym；BSPC / Num。

共 10 层：Base、Extra、Tap、Button、Nav、Mouse、Media、Num、Sym、Fun。

<details>
<summary>展开全部层与组合键</summary>

![Tako 全部键位](keymap-drawer/tako.svg)

</details>

<!-- keymap:end -->

键位图由 `.github/workflows/draw_keymap.yaml` 在每次分支 push 后重新生成，有变化时自动提交到同一分支；也可在 Actions 手动运行 **Draw ZMK keymaps**。直接解析 `tako.keymap` 及其包含的 Miryoku 宏，物理布局与固件共用 `tako-layouts.dtsi`。Studio 中保存的动态改键不在仓库里，不会反映到图中。

本地重绘（Python 3.12+）：

```sh
python -m pip install -r scripts/requirements-keymap.txt
python scripts/draw_keymap.py
```

图中的 Media 蓝牙配置键只在保留的 BLE 左主模式生效；2.4G 模式没有主机蓝牙配置，这些键为无操作。其余 Base 字母、长按和组合键共用同一份 Miryoku。

## 六个编译产物

| 文件 | 模式 / 用途 |
|---|---|
| `settings_reset.uf2` | 清空 nice!nano 设置，正常迁移不用刷 |
| `tako_dongle.uf2` | 新 2.4G dongle 主设备，USB HID + Studio，连接两半 |
| `tako_left_dongle.uf2` | 新 2.4G 左从设备，固定 slot 0 |
| `tako_right_dongle.uf2` | 新 2.4G 右从设备，固定 slot 1 |
| `tako_left_central.uf2` | 保留的 BLE 左主设备，左半 USB 接电脑 |
| `tako_right_peripheral.uf2` | 保留的 BLE 右从设备，连接左主 |

本分支的两份右侧固件使用不同无线协议，**不能混用**。2.4G 模式必须使用 dongle 和两个 `*_dongle` 半边组成的三件套。BLE 模式用 `*_central` + `*_peripheral`，不使用 dongle。本分支没有生成原 BLE dongle 三件套；要用它请回到 `codex/tako-dongle`。

## 首次刷入

1. 先分别 USB 连接两半，执行 `ec KSCAN calibration export` 备份现有 EC 校准值。
2. 三台分别刷入对应的正常 2.4G 固件：`tako_dongle.uf2`、`tako_left_dongle.uf2`、`tako_right_dongle.uf2`。
3. dongle 接电脑，两半开机。地址、信道和组密钥相同且编号唯一的设备会自动握手；这个版本不需要 BLE 配对，也不需要旧的 clear-bonds 固件。
4. 两半 nice!view 应显示连接状态及本半数字电量；dongle OLED 显示层、组合键输入状态及两半电量。测试左右字母、Home Row Mods、跨侧组合、媒体键、鼠标键和 Studio 改键。
5. 断开或重启一半时，dongle 应在约 2 秒后释放该侧按键；重启 dongle 后两半应自动重新握手。两半仍按原配置在闲置 15 分钟后深度睡眠，需按各半 reset 恢复。

**不要给两半刷 settings_reset**：它会擦除 EC 校准。蓝牙绑定仍保留在 NVS，ESB 不使用这些绑定；新协议也不改变 `zmk/ec/cal/KSCAN` 校准路径。

## 固定组网参数与密钥

共同配置在 `config/tako_2g4.conf`，build.yaml 为左右分别指定 `CONFIG_ZMK_SPLIT_ESB_SLOT=0/1`。默认信道 76（2476 MHz）、TX 0 dBm、闲置心跳 200 ms、掉线超时 2000 ms。

当前仓库提供的是**公开开发密钥，仅用于实验组网**，不是保密配置。要用于实际保密通信，应通过不提交到仓库的额外 `.conf` / 私有构建配置覆盖 `CONFIG_ZMK_SPLIT_ESB_KEY` 为随机 16 字节（32 位十六进制），并给三台使用同一把密钥。不要把私人密钥提交到公开 Git 仓库或输出到公开构建日志。同一接收器组还需使用相同 address/channel；不同键盘组应使用不同地址和密钥。

传输按每侧完整按键 bitmap 同步，同时保留每次按下/松开的队列，只有收到认证后的应用 ACK 才移除当前事件。支持键位、数字电量、输入/传感器事件和下行行为、层、HID 指示灯、物理布局命令。128 个全局键位，协议最多 8 个设备编号；当前 Tako 只配置并编译两半，未进行 8 台设备射频测试。

接收器是一个射频通道，多设备通过不同地址区分并用重传/随机退避处理碰撞；没有实现时分调度或跳频。持续过载造成队列溢出时会同步最新实体状态，期间的快速点按可能丢失。断开期间的输入不会在重连后补打一串旧字符。

## 校准和诊断

两半保留 USB 校准 Shell，校准时 USB **直接连接对应半边**，不能经 dongle 转发。Studio 在 dongle；BLE 左主模式的 Studio 在左半。

```text
ec KSCAN calibration start
ec KSCAN calibration save
ec KSCAN calibration export
ec KSCAN calibration load
esb status
```

`esb status` 输出设备编号、连接状态、发包成功/失败、收到/无效包计数和队列占用，不输出密钥。如果有延迟或漏键，分别记录左右半的结果。此命令只存在于 ESB 固件；USB 校准 Shell 已具备该命令，dongle 默认只开启 Studio 端口。

原 GPIO、MUX、ADC、50% 触发值、10/30 ms 扫描、2 ms 上电等待、断电前控制脚归零、校准存储和 15 分钟睡眠均保留。新增 ESB 驱动在 PTX 空闲时释放自己的 HFXO 请求，dongle 则持续接收。实际续航与电流还要测量，不能从编译结果推断。

## 本地与远程构建

独立本地入口（已配置，不覆盖此前 BLE 产物）：

```sh
cd /Users/gofran/Documents/workspace/keyboard/zmk-local
./build-tako-2g4.sh all        # 六个产物
./build-tako-2g4.sh dongle-set # 仅 2.4G 三件套
./build-tako-2g4.sh wired-set  # BLE 左主右从
```

输出在 `zmk-local/firmware/2g4/`。代码从旁边 `zmks/app` 读取；使用独立的 `2g4-modules/zmk-studio-messages` 和 `esb-*` 构建目录。`build-tako.sh` 原入口用于以前的环境；本实验分支优先使用上述独立入口。

`config/west.yml` 固定 zmks 新实现提交 `06ac84443992c0931b77301eae220dd44bf32feb`，来自 `https://gitee.com/gofranchang/zmks.git`。该源码的 west.yml 固定 ph-design Studio messages `8201f24`，Zephyr 保持 v3.5.0+zmk-fixes；EC 驱动仍为 `3cb9978`，dongle display 仍为 v0.3 兼容版本。

首次远程构建前，先推送源码分支，再推配置分支，让 Actions 能下载固定提交：

```sh
git -C /Users/gofran/Documents/workspace/keyboard/zmks push -u origin codex/2g4-split
git -C /Users/gofran/Documents/workspace/keyboard/tako-config push -u origin codex/tako-2g4-dongle
```

## 已验证与待验证

- 生产协议代码的主机测试：8 个独立编号、认证/错误密钥、逐字节篡改、乱序/重复、两端重启握手、超时后挑战更新、松键恢复、发布部分失败后的重试、完整下行命令尺寸。ASan/UBSan 通过；另用独立 AESCCM 实现验证了 C 生成的报文。
- 六目标本地编译通过，最终 ESB 三件套另行编译通过；配置核对包括 ESB 三端无 BT、左右 slot 0/1、dongle 为唯一 ESB 主控、保留 EC 存储/数字电量/深睡，以及另两个 BLE 角色仍可构建。
- 待实机确认：射频收发与碰撞重试时序、快速跨侧输入、断连/重启/深睡后的恢复、OLED 显示、Studio 浏览器交互及电流/续航。当前属于实验实现，不宣称已达到日常稳定使用状态。
