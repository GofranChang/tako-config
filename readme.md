# Tako ZMK — Analog Matrix EC 驱动

> 代码生成说明：本仓库的项目代码由 ChatGPT 生成。上游依赖及引用源码保留各自的作者信息和许可证。

本分支基于 `pete_driver` 的 `22c0556`，使用 [petejohanson/analog-matrix-module](https://github.com/petejohanson/analog-matrix-module)，固定提交 `a2c73edfcee7d7e626d323982b14dcd1e2b88332`。使用其中的 EC 驱动，ZMK 保持 v0.3（Zephyr v3.5.0+zmk-fixes）。上游仅支持 Zephyr 4.1 及更早版本，不要单独升级到移除 KSCAN API 的 Zephyr 版本。

保留当前左右 GPIO、MUX 通道顺序、ADC、屏幕、34 键 Miryoku、矩阵变换、左半 Studio，以及 15 分钟深度睡眠。左右半都包含正常键盘功能和 USB 校准功能。

## 构建配置

`config/west.yml` 引入新模块，替换 `ec-support-zmk-module`。`build.yaml` 使用新 `zmk-analog-matrix-calibrator` snippet；USB Shell 的独立 CDC 节点定义在 `tako.dtsi` 中。左半另保留 `studio-rpc-usb-uart`，校准与 Studio 使用不同端口。

共享功能改用 `CONFIG_ZMK_ANALOG_MATRIX_SETTINGS`、`CONFIG_ZMK_ANALOG_MATRIX_VERBOSE_CALIBRATOR` 和 `CONFIG_ZMK_ANALOG_MATRIX_DYNAMIC_POLL_RATE`。仍保留 `CONFIG_ZMK_KSCAN_EC_MATRIX_FAKE_OPEN_DRAIN=y`，使放电引脚通过输入/输出切换工作。

**固定版本的上游 EC 源码仍使用旧 `CONFIG_ZMK_KSCAN_EC_MATRIX_DYNAMIC_POLL_RATE` 宏初始化动态扫描时间字段，因此同时保留该旧兼容开关。** 它会产生弃用提示，但当前不能只设置新开关，否则动态扫描参数会变为零。这里不修改上游驱动源码。

## 首次刷入与校准

新模块把 Flash 路径从 `zmk/ec/calibration/KSCAN` 改为 `zmk/ec/cal/KSCAN`，旧数据不会自动加载。刷入新固件后，两半都需重新校准和保存。切换前可以用旧固件导出一份校准备份；不必为迁移刷 `settings_reset`。

1. 通过 GitHub Actions 构建并给每半刷入对应 UF2。
2. 用 USB 直接连接要校准的半边。串口波特率为 115200；左半有 Studio 与校准两个 CDC 端口，选择有 Shell 提示符的那个。
3. 输入 `ec` 查看矩阵名；通常为 `KSCAN`。
4. 输入 `ec KSCAN calibration start`。低值采样时全部松开；高值采样时逐颗按到底并保持，收到该键完成提示后松开，共 17 颗。
5. 出现 `Calibration complete!` 后，输入 `ec KSCAN calibration save`。上游没有明确的 Flash 写入成功回执，需检查日志、重启后读取并验证按键。
6. 输入 `ec KSCAN calibration export` 可导出两组 DTS 数组；`ec KSCAN calibration load` 可读取新路径保存的数据。两组数组仍各 20 项，按行排列，每行 5 列。
7. 左右半分别进行以上操作。即使日常使用 dongle，校准也要直接连接左右半的 USB。

**逐键日志坐标改为 `(MUX 列索引, strobe 行索引)`，旧驱动为 `(行,列)`。** 使用配套上位机时选择 `analog-matrix（新分支）` 协议；直接使用旧解析方式会导致进度图错位。Shell 命令与 DTS 数组顺序保持一致。

固定版本的新 EC 校准器要求按压读数至少达到 `avg_low + 128`（10 位 ADC 的半量程除以 4），不再固定要求绝对读数达到 512。校准时仍应充分按到底。

## 触发与休眠

`trigger-percentage = <50>` 为每键校准范围中点。上游计算原始触发值为 `high - max(range × percentage / 100, noise)`；增大 percentage 会降低原始触发阈值。释放滞回由范围和噪声决定。

保留 10 ms 活动扫描、30 ms 空闲扫描、3000 ms 慢速扫描和 2 ms 上电等待。ZMK 在 15 分钟无操作后深度睡眠；睡眠时停止扫描，按各半实体 reset 重启，启动时加载新路径下的校准数据。没有 ADXL362，保持 `CONFIG_SLEEP_AWAKE_TRIGGER=n`。

`settings_reset` 和清除设置的键位会擦除校准数据，使用后需重新校准。

## 回退

回到 `pete_driver`，构建并刷入旧模块固件。旧路径中的校准数据在未清除设置时仍保留；如旧固件无法恢复按键，重新校准旧驱动。双方的 Flash 校准路径独立。
