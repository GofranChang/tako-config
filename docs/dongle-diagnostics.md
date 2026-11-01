# Dongle 输入失效诊断

2026-10-25 实机反馈：触控板初始可用，滑动数次后触控板和右半按键都失效；dongle 动画继续，重启 dongle 可恢复。接收端输入栈增至 2048 字节后仍复现，因此栈不足没有被证实为根因。

可选 shield `tako_dongle_diagnostics` 在 OLED 上覆盖诊断文字，不改变蓝牙时序、当前配置的事件队列大小、坐标处理器或屏幕方向；不增加串口或日志接口。计数探针转发原调用并保留返回值。普通七种固件不启用这些探针。

只需刷诊断版 dongle，不清绑定，不改右侧校准。建议先观察静止时的数据，再滑动并穿插按键；出现失效后继续滑动、按键约 5 秒，再拍下整个诊断屏幕。方便的话同时留一张正常时的照片。诊断版没有自动重启或自动断线重连，保留现场。

| 字段 | 含义 |
| --- | --- |
| B / D / R | 当前全部 BLE 连接数 / 断开累计次数 / 最近断开原因（十六进制）；B 包含可能连接的电脑，不仅是键盘 |
| USB 或 BLE | 当前实际选择的主机输出通道 |
| K / M | 从 split 接收队列取出、准备分发的按键位置事件 / 触控板事件累计数；不等于底层无线包计数 |
| I / Q | 输入线程触控板回调累计次数 / split 输入转发失败次数（队列满等） |
| U / E | USB HID 提交成功次数 / 返回错误次数；提交成功不代表主机已收到 |
| P / ERR | 尚未返回的 USB HID 发送调用数 / 最近一次发送错误的负 errno |
| W / T | 系统工作队列心跳 / 开机秒数（均每秒增加，屏幕每 500 ms 采样） |
| USB: | ZMK 当前 USB 状态枚举数值，供结合该版本源码分析 |

事件数仅用于比较是否增长，超过 99999 后回绕。I 回调与原 listener 的先后次序由链接顺序决定，不代表完整处理成功。

初步判读：

- W 停、T 走：系统工作队列未正常执行，需继续查阻塞者。
- W 正常、滑动时 M 停：事件没有到达分发处，结合 B/D/R 继续查 BLE、订阅、右侧发送或队列丢失。
- M 继续、I 停或 Q 增长：输入队列/线程路径异常。
- M/I 继续，E 增长或 P 持续不归零：重点检查 USB 上报路径和 ERR。
- 输出变成 BLE：先检查输出切换和主机连接，不能直接称为 split 断联。

这些指标用于缩小范围，不能仅凭单个计数认定根因。

## 本地编译

在已经配置好的 `zmk-local` 环境中：

```sh
cd /Users/gofran/Documents/workspace/keyboard/zmk-local
source .venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR="$PWD/zephyr-sdk-0.16.8"
export ZEPHYR_BASE="$PWD/zephyr"
export Zephyr_DIR="$PWD/zephyr/share/zephyr-package/cmake"
west build zmk/app -d build/tako_dongle_diagnostics -b nice_nano_v2 -p always \
  -S studio-rpc-usb-uart -- \
  '-DSHIELD=tako_dongle dongle_display tako_cirque_receiver tako_dongle_diagnostics' \
  -DZMK_CONFIG=/Users/gofran/Documents/workspace/keyboard/tako-config/config \
  -DCONFIG_ZMK_STUDIO=y
```

输出为 `build/tako_dongle_diagnostics/zephyr/zmk.uf2`。诊断 shield 未加入默认 `build.yaml`，普通远程编译仍输出原来的七种固件。需要远程构建诊断版时，在 dongle 条目的 `shield` 末尾追加 `tako_dongle_diagnostics` 即可。

本地诊断版编译通过，并检查 ELF：五处 wrap 函数均有调用点、保留原函数；正式 dongle 构建中不应存在这些 wrap 符号。实际失效现场仍待采集。

## 旧驱动历史：右侧生产者工作队列修正

本节记录旧驱动的排查与修正。当前 Sweep-Pro 分支使用 `nxtkb/cirque-input-module` 的原生 `K_NO_WAIT` 输入上报，已移除这里描述的独立队列补丁；诊断计数功能保留。

现场照片为 B1/D0/R00、M860/I860/U430、Q0/E0/P0；用户随后确认持续滑动时 M/I/U 不再增长，只有 W/T 继续。这说明 dongle 系统工作队列仍在运行，现有探针没有记录到断连或 USB 错误；不能仅凭这些计数区分右侧停止发送、无线订阅路径停滞等原因。K0 表示未收到任何按键事件，未校准状态下不能据此认定按键与触控板在同一时刻停摆。

固定版本源码存在以下潜在循环等待：

1. Cirque `pinnacle_gpio_cb()` 把读取工作提交到右半系统工作队列。
2. 读取工作使用 `input_report_* (..., K_FOREVER)`，输入队列满时阻塞系统工作队列。
3. 输入线程经 split input 直接执行 BLE notify，可能等待 TX context 或 ATT/L2CAP 资源。
4. Zephyr 的 HCI 完成处理把 `conn->tx_complete_work` 提交到系统工作队列，`tx_notify()` 在那里回收 TX context 并执行 ATT 完成回调；系统工作队列阻塞后资源无法及时回收。

修正是在 `tako_cirque_right` 构建中启动一个独立 Cirque 工作队列（2048 字节栈、优先级 5），仅把上游 `input_pinnacle.c` 的 `k_work_submit()` 改送该队列。仍使用原驱动和阻塞式输入上报，不通过丢弃按键/点击事件来绕开问题。编译器只对该源文件强制包含 `cirque_work_queue.h`；未修改依赖仓库，不全局重定向内核函数。构建在模块注册完成后配置该源文件，找不到唯一驱动源时立即报错，避免静默失效。

本地完整编译 `tako_right_cirque` 通过；编译数据库确认仅 Pinnacle 源文件带此头文件，最终反汇编确认 GPIO 回调提交到 `tako_cirque_work_q`，且右半最终 `.config` 与上版完全相同。实际是否解决现场问题仍需用户实测。

本次只需更新右半 `tako_right_cirque.uf2`；dongle 保留诊断版，便于观察 M/I/U 是否持续增长。不清绑定、不重置校准，不改 GPIO、方向、灵敏度或睡眠配置。验证通过后可给 dongle 刷回普通界面。

## 右侧画圈卡顿诊断（可选）

当小圈移动也不连续、停手后不追着移动，先测量采样与传输，而不继续扩大队列或调整速度。额外 shield `tako_cirque_probe` 只供右侧 Cirque 使用，增加 `cirque_probe` 校准串口诊断命令。默认 `build.yaml` 七种固件不启用它；不增加后台定时器，默认不改变触控参数、工作队列、BLE 参数或校准；V2 的显式命令可以临时修改起步门槛及 BLE latency，重启还原。每次输入只更新少量计数，发送包装函数只转发一次调用并保留返回值。

本地诊断产物：`../zmk-local/firmware/diagnostics/tako_right_cirque_probe.uf2`。只刷右半；dongle 和左半保持当前固件。**测试时 dongle 继续插着，右半同时用 USB 接到 Mac**，USB 仅用于读取校准串口，光标仍通过 dongle 输出。无需清绑定或重新校准。刷普通 `tako_right_cirque.uf2` 即可移除诊断。

在已配置的本地 west 环境中构建：

```sh
west build zmk/app -d build/sweep-pro-cirque-probe -b nice_nano_v2 -p always \
  -S zmk-analog-matrix-calibrator -- \
  '-DSHIELD=tako_right tako_cirque_right tako_cirque_probe' \
  '-DZMK_CONFIG=/Users/gofran/Documents/workspace/keyboard/tako-config/config'
```

采样流程：

1. 串口执行 `cirque_probe reset`，仅清诊断计数。
2. 在触控板中央连续画圈约 10 秒，不按 Mouse 层 Z，避免边缘自动移动干扰读数。
3. 抬手后执行 `cirque_probe show`，保存输出。

输出标记 `TAKO_CIRQUE_PROBE_V2` 用于确认刷入支持运行时对照的诊断固件（旧 V1 仅支持计数）。计数含义：

| 字段 | 含义 |
| --- | --- |
| irq / work / read / read_err | DR 中断、读取工作、成功取样及失败次数；irq 明显多于 work 说明有工作合并，不能直接等同于蓝牙丢包 |
| contact / low_z | 非零压力样本数、低于触摸门槛的样本数；low_z 多可能反复中断触摸状态 |
| generated_frames / nonzero_frames | 产生的 X/Y 同步帧数，以及其中位移非零的帧数；不是 USB 上报频率 |
| dispatched_frames / queued / dropped | 输入线程处理的帧数、成功入队的相对事件数及入队失败数；不是同一计数单位 |
| tx_events / tx_err / max_tx_us | 向 split 传输提交的输入事件数、失败数及最慢一次提交耗时；提交成功不代表主机已收到 |
| max_irq_wait_us | 最长一次 DR 中断到读取工作开始的等待时间 |
| frame_gaps_us | 产生 X/Y 帧的时间间隔分布；起止停顿也可能落在大间隔一栏，需结合连续滑动的主要分布判断 |
| BLE interval_us / latency | 实际协商的连接间隔和 peripheral latency；latency 是允许跳过的连接事件数，不能直接当作每帧延迟 |
| feed / sample_rate_reg / read_rc | 芯片模式、采样率寄存器原值及读取结果；feed 的 bit1 为绝对模式标记 |

这里只诊断到右半向 BLE 提交事件的路径。若读数正常而光标仍不连续，再使用 dongle 端计数核对接收和 USB 上报。

### 起步和 BLE 延迟感对照（V2）

只需更新右侧诊断固件一次，之后通过同一串口逐项测试。默认参数保持现有配置，以下改动均不写入设置或校准，重启右半即可恢复。

- 手指抬起后执行 `cirque_probe tap20`：将软件轻触判定/光标起步位移门槛由 80 临时改为 20；速度、加速曲线及 BLE 参数不变。门槛降低可能让轻触点击更容易被识别成移动，需同时测试点击。`cirque_probe tap80` 恢复配置值。
- 恢复门槛后执行 `cirque_probe latency0`：仅向已连接的 dongle 请求 peripheral latency=0，连接间隔和超时保持实际值。命令返回 0 仅表示请求成功，请随后执行 `cirque_probe show` 确认实际 latency 已变为 0。`cirque_probe latency30` 请求恢复当前基线 30。
- latency=0 可能增加无线功耗；它是临时对照，不作为续航结论。起步门槛和 latency 必须分开改变、分开比较，避免无法归因。

V2 还显示 `dispatch_gaps_us`（输入线程处理帧的间隔）和 `tx_wait_us`（每次提交的等待时间）分布。

V2 的 `tap_movement_limit` 显示当前有效门槛，`BLE latency` 显示实际协商值。如果改变 latency 的请求失败，保留错误码，不将其视为已生效。

### 实机结果与 TX8 对照

2026-11-01：运行时对照确认，门槛 20 比 80 起步更跟手，但持续移动仍有延迟感。仅将实际 BLE latency 从 30 改为 0 没有明显改善，已恢复为 30。

门槛 20 / latency 30 一轮中，1052 次发送有 40 次等待 32–64 ms，最长约 57.8 ms；526 个生成帧无队列丢弃。门槛 80 / latency 0 一轮中，3166 次发送有 154 次等待 32–64 ms、2 次超过 64 ms，最长约 112.5 ms，仍无队列丢弃。两个动作序列和时长不同，不能据此断言 latency=0 更差；但它没有消除发送等待。输入线程处理间隔明显不均，而传感器读取主要保持 8–16 ms，发送资源不足是下一项待验证假设。

可选 `tako_cirque_tx8` shield 将 `BT_L2CAP_TX_BUF_COUNT`、`BT_BUF_ACL_TX_COUNT`、`BT_CONN_TX_MAX` 从 3 同时改为 8，并保留已确认改善起步的门槛 20；不改变 PHY、连接间隔、peripheral latency、采样率或应用事件队列。它只增加可用发送资源，不保证解决空中传输或接收端瓶颈。普通七份固件保持不变。

诊断产物：`../zmk-local/firmware/diagnostics/tako_right_cirque_tx8.uf2`，只刷右半，保留 dongle 和 USB 诊断连接。由四个 shield 构建：`tako_right tako_cirque_right tako_cirque_probe tako_cirque_tx8`。刷后用 `cirque_probe show` 确认默认门槛 20、实际 latency 30，再清零计数做连续画圈比较。恢复原诊断版或普通右侧 Cirque UF2 即可撤销。
