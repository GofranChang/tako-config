#!/usr/bin/env python3
"""Render the source keymap and refresh the generated README section."""
import html
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

import yaml

ROOT = Path(__file__).resolve().parents[1]
CONFIG = "keymap_drawer.config.yaml"
SOURCE = "config/boards/shields/tako/tako.keymap"
LAYOUT = "config/boards/shields/tako/tako-layouts.dtsi"
OUTPUT = ROOT / "keymap-drawer"
START = "<!-- keymap:start -->"
END = "<!-- keymap:end -->"


def keymap(*args, input_text=None):
    result = subprocess.run(
        [sys.executable, "-m", "keymap_drawer", "-c", CONFIG, *args],
        cwd=ROOT, capture_output=True, text=True, input=input_text,
    )
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    result.check_returncode()
    return result.stdout


def flatten(keys):
    return [key for row in keys for key in (row if isinstance(row, list) else [row])]


def legend(key):
    if isinstance(key, str):
        value = key or "—"
    else:
        value = key.get("t", "") or "—"
        if key.get("h"):
            value += " / " + key["h"]
    return html.escape(value).replace("|", "&#124;")


def main():
    readme = ROOT / "readme.md"
    original = readme.read_text(encoding="utf-8")
    if original.count(START) != 1 or original.count(END) != 1:
        raise ValueError("README must contain exactly one keymap:start/end marker pair")
    start, end = original.index(START), original.index(END)
    if start >= end:
        raise ValueError("README keymap markers are reversed")

    parsed = yaml.safe_load(keymap("parse", "-z", SOURCE))
    layers = parsed["layers"]
    if not layers or "Base" not in layers:
        raise ValueError("Source keymap must define a Base layer")
    for name, keys in layers.items():
        if len(flatten(keys)) != 34:
            raise ValueError(f"{name}: expected exactly 34 keys")
    for combo in parsed.get("combos", []):
        if not all(0 <= pos < 34 for pos in combo["p"]):
            raise ValueError("Combo references a key outside the 34-key layout")
        # Keep thumb combos below their keys so they do not cover tap/hold labels.
        if set(combo["p"]).issubset({30, 31, 32, 33}):
            combo.update(align="bottom", offset=0.5)

    parsed["layout"] = {"dts_layout": LAYOUT}
    OUTPUT.mkdir(exist_ok=True)
    data_path = OUTPUT / "tako.yaml"
    data_path.write_text(yaml.safe_dump(parsed, sort_keys=False, allow_unicode=True), encoding="utf-8")
    for filename, extra in (("tako.svg", []), ("tako-base.svg", ["--select-layers", "Base"])):
        if filename == "tako-base.svg":
            base_data = parsed | {"draw_config": {"n_columns": 1}}
            svg = keymap("draw", "-", *extra, input_text=yaml.safe_dump(base_data, sort_keys=False))
        else:
            svg = keymap("draw", str(data_path.relative_to(ROOT)), *extra)
        # A solid background keeps the same contrast in light/dark README themes.
        svg = svg.replace("\n", '\n<rect width="100%" height="100%" fill="#ffffff"/>\n', 1)
        ET.fromstring(svg)  # Do not replace the README with a broken drawing.
        (OUTPUT / filename).write_text(svg, encoding="utf-8")

    base = [legend(key) for key in flatten(layers["Base"])]
    rows = ["| " + " | ".join(base[i:i + 10]) + " |" for i in range(0, 30, 10)]
    table = "\n".join([
        "| 左1 | 左2 | 左3 | 左4 | 左5 | 右1 | 右2 | 右3 | 右4 | 右5 |",
        "|---|---|---|---|---|---|---|---|---|---|", *rows,
    ])
    content = "\n\n" + "\n\n".join([
        "![Tako Base 层](keymap-drawer/tako-base.svg)",
        "主字为点按，下方为长按；表格中的 `/` 分隔点按与长按。`2x` 表示双击切层；右上角标注为 Shift 功能。组合键框连接的按键需要同时按下。",
        table,
        "拇指键（左至右）：" + "；".join(base[30:]) + "。",
        f"共 {len(layers)} 层：" + "、".join(layers) + "。",
        "<details>\n<summary>展开全部层与组合键</summary>\n\n![Tako 全部键位](keymap-drawer/tako.svg)\n\n</details>",
    ]) + "\n\n"
    readme.write_text(original[:start + len(START)] + content + original[end:], encoding="utf-8")
    print(f"Rendered {len(layers)} layers, {len(parsed.get('combos', []))} combos; updated readme.md")


if __name__ == "__main__":
    main()
