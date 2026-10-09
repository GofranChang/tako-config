# Tako ZMK config — EC module migration

This config uses [petejohanson/ec-support-zmk-module](https://github.com/petejohanson/ec-support-zmk-module), pinned to `bcb61bdee0a548976bea8f376f8cdec6b4ffd5fe`, with ZMK v0.3.

The existing Miryoku keymap, matrix transform, ADC settings, screen wiring, and left-side Studio support are retained. The legacy ssbb driver and its device-tree binding have been removed. The EC driver is provided by the pinned external module in `config/west.yml`.

## First flash and calibration

The old absolute 650/600 thresholds are not calibration data for this driver. Neither half will report keys until it has valid high/low calibration values. Calibrate both halves separately before using the keyboard.

1. Build through GitHub Actions and flash the matching left/right UF2 to each half. Keep a copy of your last working ssbb UF2 for rollback.
2. Connect one half by USB. Open its USB serial shell at 115200 baud. On the left there are two CDC ports: Studio and the shell; choose the one with the shell prompt.
3. Type `ec` and use Tab completion to find the matrix device name. It is expected to be `KSCAN` (the overlay label).
4. Run `ec KSCAN calibration start`. Leave every key released during low sampling. When high sampling starts, slowly press each of the 17 keys on this half fully and release it when its calibration is acknowledged. The verbose output identifies positions by `(strobe,input)`, matching matrix `(row,column)`.
5. After `Calibration complete!`, run `ec KSCAN calibration export` and keep the output as a backup. Run `ec KSCAN calibration save`, then reboot and check that all keys work. Saved values are loaded from flash on boot.
6. Repeat on the other half. If Tab completion gives a different device name, substitute it in the commands above.

The three unused positions in row 3 are masked. `trigger-percentage = <50>` starts at the midpoint of each calibrated range. The pinned driver computes its raw press limit as `high - range * percentage / 100`; increasing this value lowers the raw trigger threshold. Tune only after calibration and a typing check. Hysteresis is derived from the range and measured noise.

The scan intervals initially retain the old 10 ms active / 30 ms idle / 3000 ms sleep behavior, with a 2 ms matrix power warm-up. ZMK deep sleep is enabled after 15 minutes of inactivity (`CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=900000`). The EC scanner stops scanning in deep sleep; press the physical reset button on each sleeping half to restart it. Saved EC calibration is loaded again on boot. Actual performance, power consumption, and wake behavior need checking on the keyboard. No ADXL362 wake-up hardware is configured, so `CONFIG_SLEEP_AWAKE_TRIGGER=n` is required.

`settings_reset` erases stored settings, including EC calibration. Recalibrate both halves after using it. Existing Miryoku keys that reset settings have the same effect.

## Rollback

Flash your previous working ssbb firmware to both halves. Its thresholds are compiled into that firmware. For a source rollback, restore the previous overlays, `config/west.yml`, `build.yaml`, and half configs together.
