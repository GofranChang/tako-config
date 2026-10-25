# Dongle 输入失效诊断

2026-10-25 实机反馈：触控板初始可用，滑动数次后触控板和右半按键都失效；dongle 动画继续，重启 dongle 可恢复。接收端输入栈增至 2048 字节后仍复现，因此栈不足没有被证实为根因。

可选 shield `tako_dongle_diagnostics` 在 OLED 上覆盖诊断文字，不改变蓝牙时序、事件队列大小、1.5 倍坐标缩放或屏幕方向；不增加串口或日志接口。计数探针转发原调用并保留返回值。普通七种固件不启用这些探针。

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
