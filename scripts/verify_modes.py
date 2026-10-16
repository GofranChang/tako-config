#!/usr/bin/env python3
"""Verify actual compiled Tako keymaps: run with Zephyr's devicetree on PYTHONPATH.

Usage: python scripts/verify_modes.py BUILD/zephyr/edt.pickle [...]
Only load pickle files produced by your trusted local Zephyr build.
"""
import pickle
import sys
from pathlib import Path

LAYERS = ['BASE', 'EXTRA', 'TAP', 'BUTTON', 'NAV', 'MOUSE', 'MEDIA', 'NUM', 'SYM', 'FUN']
KEY = 0x70000
CTRL, GUI = KEY | 0xE0, KEY | 0xE3


def binding(node, position):
    value = node.props['bindings'].val[position]
    return value.controller.labels[0], tuple(value.data.values())


def verify(path):
    with path.open('rb') as source:
        edt = pickle.load(source)
    layers = list(edt.get_node('/keymap').children.values())
    assert [n.name for n in layers] == LAYERS + ['MAC_' + n for n in LAYERS]
    for index, node in enumerate(layers):
        assert len(node.props['bindings'].val) == 34, node.path
        assert node.props['display-name'].val.startswith('MAC ' if index >= 10 else 'WIN ')
        for value in node.props['bindings'].val:
            name = value.controller.labels[0]
            if name == 'u_lt':
                target = value.data['param1']
                assert (10 <= target < 20) if index >= 10 else (0 <= target < 10)
            if name.startswith(('u_to_U_', 'tako_mac_to_')):
                guard = value.controller.props['bindings'].val
                assert guard[0].controller.labels[0] == 'none'
                assert guard[1].controller.labels[0] == 'to'
                target = guard[1].data['param1']
                assert (10 <= target < 20) if index >= 10 else (0 <= target < 10)

    # Letter taps and thumb taps stay identical, with mode-local hold layers.
    for index in (0, 1):
        win, mac = layers[index], layers[index + 10]
        for position in range(34):
            wn, wd = binding(win, position)
            mn, md = binding(mac, position)
            assert wn == mn
            if wn == 'u_mt':
                assert wd[1] == md[1]
                assert md[0] == {CTRL: GUI, GUI: CTRL}.get(wd[0], wd[0])
            elif wn == 'u_lt':
                assert md == (wd[0] + 10, wd[1])
            else:
                assert wd == md
        for position in (12, 17):
            assert binding(win, position)[1][0] == CTRL
            assert binding(mac, position)[1][0] == GUI
        for position in (10, 19):
            assert binding(mac, position)[1][0] == CTRL

    # No accidental GUI/Control reversal inside explicit clipboard shortcuts.
    win_clip = [0x0107001C, 0x01070019, 0x01070006, 0x0107001B, 0x0107001D]
    mac_clip = [0x0A07001D, 0x08070019, 0x08070006, 0x0807001B, 0x0807001D]
    for index in (4, 5):
        for offset, expected in ((0, win_clip), (10, mac_clip)):
            assert [binding(layers[index + offset], p)[1][0] for p in range(5, 10)] == expected
    for offset, expected in ((0, win_clip), (10, mac_clip)):
        assert [binding(layers[3 + offset], p)[1][0] for p in range(5, 10)] == expected
    assert binding(layers[4], 25) == ('kp', (KEY | 0x4A,))
    assert binding(layers[14], 25) == ('kp', (0x08070050,))
    assert binding(layers[4], 28) == ('kp', (KEY | 0x4D,))
    assert binding(layers[14], 28) == ('kp', (0x0807004F,))
    for index, modifier in ((4, 0x01000000), (14, 0x04000000)):
        assert binding(layers[index], 4) == ('kp', (modifier | KEY | 0x50,))
        assert binding(layers[index], 14) == ('kp', (modifier | KEY | 0x4F,))
    for index, target in ((6, 10), (16, 0)):
        assert binding(layers[index], 9) == ('to', (target,))
        assert binding(layers[index], 4) == ('none', ())
        assert binding(layers[index], 29) == ('out', (0,))

    combos = edt.get_node('/combos').children
    assert len(combos) == 24
    for name, win in combos.items():
        if not name.startswith('thumbcombos_'):
            continue
        mac = combos[name.replace('thumbcombos_', 'tako_mac_thumb_')]
        assert mac.props['layers'].val == [v + 10 for v in win.props['layers'].val]
        assert mac.props['key-positions'].val == win.props['key-positions'].val
        wn, wd = binding(win, 0)
        mn, md = binding(mac, 0)
        assert wn == mn
        assert md == ((wd[0] + 10, wd[1]) if wn == 'lt' else wd)
    print(f'{path}: 20 layers / 34 positions / 24 combos; mode bindings verified')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for filename in sys.argv[1:]:
        verify(Path(filename))
