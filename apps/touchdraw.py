#!/usr/bin/env python3
"""Minimal touch drawing demo for the SuziePi 3.5" screen.

Draws on /dev/fb1 (480x320 RGB565) and reads taps from the ADS7846 evdev node.
Touch the colored boxes along the top to pick a color; "CLR" clears the canvas.
Run on the Pi as root (or a user in the video and input groups):

    sudo python3 touchdraw.py            # runs until Ctrl-C
    sudo python3 touchdraw.py --seconds 20
"""
import argparse
import mmap
import select
import struct
import time

W, H = 480, 320
FB = "/dev/fb1"
EVENT_FMT = "llHHi"
EVENT_SIZE = struct.calcsize(EVENT_FMT)
EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
ABS_X, ABS_Y = 0, 1
BTN_TOUCH = 330

BAR_H = 36
BRUSH = 3  # half size in pixels
PALETTE = [0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F, 0x0000]  # RGB565
BG = 0xFFFF
BOX_W = 56


def find_touch_device():
    """Return the /dev/input/eventN node of the ADS7846 touch controller."""
    name = None
    with open("/proc/bus/input/devices") as f:
        for line in f:
            if line.startswith("N: Name="):
                name = line.split("=", 1)[1].strip().strip('"')
            elif line.startswith("H: Handlers=") and name and "ADS7846" in name:
                for tok in line.split():
                    if tok.startswith("event"):
                        return "/dev/input/" + tok
    raise SystemExit("ADS7846 touchscreen not found")


def to_pixels(raw_x, raw_y):
    """Convert raw ABS values to screen pixels (see ../calibration.txt)."""
    x = (raw_x + 36) / 7.74
    y = (raw_y - 280) / 11.14
    return min(max(int(x), 0), W - 1), min(max(int(y), 0), H - 1)


class Screen:
    def __init__(self):
        self.f = open(FB, "r+b", buffering=0)
        self.mem = mmap.mmap(self.f.fileno(), W * H * 2)

    def fill_rect(self, x0, y0, x1, y1, color):
        x0, y0 = max(x0, 0), max(y0, 0)
        x1, y1 = min(x1, W), min(y1, H)
        if x1 <= x0 or y1 <= y0:
            return
        row = struct.pack("<H", color) * (x1 - x0)
        for y in range(y0, y1):
            off = (y * W + x0) * 2
            self.mem[off:off + len(row)] = row

    def clear_canvas(self):
        self.fill_rect(0, BAR_H, W, H, BG)

    def draw_bar(self, selected):
        for i, c in enumerate(PALETTE):
            x = i * BOX_W
            self.fill_rect(x, 0, x + BOX_W, BAR_H, c)
            if i == selected:  # white inner marker (black on white box)
                self.fill_rect(x + 20, 12, x + 36, 24, 0xFFFF if c != 0xFFFF else 0)
        self.fill_rect(W - 88, 0, W, BAR_H, 0x7BEF)  # CLR button (gray)
        self.fill_rect(W - 70, 10, W - 18, 26, 0x0000)  # simple "bar" glyph
        self.fill_rect(W - 68, 12, W - 20, 24, 0x7BEF)

    def dot(self, x, y, color):
        self.fill_rect(x - BRUSH, max(y - BRUSH, BAR_H), x + BRUSH + 1, y + BRUSH + 1, color)

    def line(self, p0, p1, color):
        (x0, y0), (x1, y1) = p0, p1
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(steps + 1):
            self.dot(x0 + (x1 - x0) * i // steps, y0 + (y1 - y0) * i // steps, color)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=0, help="exit after N seconds (0 = run forever)")
    args = ap.parse_args()

    scr = Screen()
    dev = open(find_touch_device(), "rb", buffering=0)
    color_idx = 0
    scr.fill_rect(0, 0, W, H, BG)
    scr.draw_bar(color_idx)

    raw_x = raw_y = None
    down = False
    last = None
    deadline = time.time() + args.seconds if args.seconds else None
    print("touch to draw; Ctrl-C to quit", flush=True)
    try:
        while deadline is None or time.time() < deadline:
            if not select.select([dev], [], [], 0.5)[0]:
                continue
            _, _, etype, code, value = struct.unpack(EVENT_FMT, dev.read(EVENT_SIZE))
            if etype == EV_ABS and code == ABS_X:
                raw_x = value
            elif etype == EV_ABS and code == ABS_Y:
                raw_y = value
            elif etype == EV_KEY and code == BTN_TOUCH:
                down = bool(value)
                if not down:
                    last = None
            elif etype == EV_SYN and down and raw_x is not None and raw_y is not None:
                x, y = to_pixels(raw_x, raw_y)
                if y < BAR_H:  # toolbar
                    if x >= W - 88:
                        scr.clear_canvas()
                    elif x < len(PALETTE) * BOX_W:
                        color_idx = x // BOX_W
                        scr.draw_bar(color_idx)
                    last = None
                else:
                    color = PALETTE[color_idx]
                    if last:
                        scr.line(last, (x, y), color)
                    else:
                        scr.dot(x, y, color)
                    last = (x, y)
    except KeyboardInterrupt:
        pass
    finally:
        scr.fill_rect(0, 0, W, H, 0x0000)


if __name__ == "__main__":
    main()
