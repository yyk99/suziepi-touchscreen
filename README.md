# SuziePi 3.5" touch screen on a Raspberry Pi

Setup notes, tools and apps for a SuziePi 3.5" SPI touch screen (480x320,
ILI9486 display controller, XPT2046 resistive touch controller) on the Raspberry
Pi `pino.home`.

## Repository layout

```
┌────────────────────────────┬─────────────────────────────────────────────────────────┐
│ Path                       │ What                                                    │
├────────────────────────────┼─────────────────────────────────────────────────────────┤
│ README.md                  │ This file: hardware setup, calibration, troubleshooting │
│ piscreen-suzie.dts / .dtbo │ Custom device tree overlay (source and compiled)        │
│ calib.py                   │ 4-corner touch calibration script (runs on the Pi)      │
│ calibration.txt            │ Raw touch values to pixel conversion formulas           │
│ apps/                      │ Apps for the screen; start with apps/README.md          │
└────────────────────────────┴─────────────────────────────────────────────────────────┘
```

## Target system

```
┌────────────┬───────────────────────────────────────────────────────────┐
│ Item       │ Value                                                     │
├────────────┼───────────────────────────────────────────────────────────┤
│ Host       │ pino.home (ssh pi@pino.home)                              │
│ Board      │ Raspberry Pi 3 Model A+                                   │
│ OS         │ Raspbian 12 (bookworm), kernel 5.10.103-v7+               │
│ Boot files │ /boot/config.txt, /boot/cmdline.txt (not /boot/firmware)  │
│ Desktop    │ None (headless, no X or Wayland installed)                │
│ Network    │ USB Ethernet (r8152); disable-wifi and disable-bt are set │
└────────────┴───────────────────────────────────────────────────────────┘
```

The screen box is labeled "Driver: ILI9486". No vendor documentation was found
for the name "SuziePi", so the configuration is based on the standard
Waveshare-style 3.5" ILI9486 + XPT2046 boards.

## Result

```
┌──────────┬───────────────────┬───────────────────────────────────────────────────────────────────┐
│ Function │ Device            │ Notes                                                             │
├──────────┼───────────────────┼───────────────────────────────────────────────────────────────────┤
│ Display  │ /dev/fb1          │ fb_ili9486, 480x320, RGB565 (16 bpp), 16 MHz SPI, rotated 90      │
├──────────┼───────────────────┼───────────────────────────────────────────────────────────────────┤
│ Touch    │ /dev/input/event0 │ ADS7846 Touchscreen (XPT2046), SPI CS1, IRQ on GPIO17             │
├──────────┼───────────────────┼───────────────────────────────────────────────────────────────────┤
│ Console  │ HDMI (fb0, tty1)  │ Deliberately not moved to the touch screen, so fb1 is free for a  │
│          │                   │ standalone app                                                    │
└──────────┴───────────────────┴───────────────────────────────────────────────────────────────────┘
```

Pin usage (standard for this board type): SPI0 (GPIO 7-11), display DC = GPIO24,
display reset = GPIO25, touch pen-down IRQ = GPIO17.

## What was changed on the Pi

### /boot/config.txt

Appended at the end:

```
# SuziePi 3.5 (ILI9486 + XPT2046)
dtoverlay=piscreen-suzie,speed=16000000,rotate=90
```

`dtparam=spi=on` was already present. The original file is saved as
`/boot/config.txt.bak-suziepi`.

### /boot/cmdline.txt

Unchanged from the original. A temporary `fbcon=map:10` was added during testing
and then removed, because the console must stay on HDMI. Backup:
`/boot/cmdline.txt.bak-suziepi`.

### /boot/overlays/piscreen-suzie.dtbo

A custom overlay, made as a copy of the stock `piscreen` overlay (same ILI9486
init sequence as the Waveshare 3.5" board) with extra touch properties added to
the `ads7846` node:

```
ti,x-min = /bits/ 16 <346>;
ti,x-max = /bits/ 16 <3864>;
ti,y-min = /bits/ 16 <172>;
ti,y-max = /bits/ 16 <3896>;
touchscreen-inverted-y;
```

The stock overlay already contains `ti,swap-xy`. Because the swap changes which
hardware axis each property applies to, the min/max pairs look "crossed" and the
inversion is `inverted-y` rather than `inverted-x`. These values were found
empirically with the corner calibration below, not derived from theory.

The overlay source and build files are in this directory (`piscreen-suzie.dts`,
`piscreen-suzie.dtbo`, `calibration.txt`, `calib.py`) and on the Pi in
`~/suziepi/`. In the decompiled `.dts` the values show as hex byte arrays (`[01
5a]` = 346, `[0f 18]` = 3864, `[00 ac]` = 172, `[0f 38]` = 3896).

To rebuild it:

```sh
dtc -I dtb -O dts /boot/overlays/piscreen.dtbo -o ps.dts
# add the properties above after the "ti,pressure-max" line, then:
dtc -I dts -O dtb ps.dts -o piscreen-suzie.dtbo
sudo cp piscreen-suzie.dtbo /boot/overlays/
sudo reboot
```

## Verifying

```sh
# Display driver bound (expect: fb1 fb_ili9486 480x320 ... at 16 MHz)
dmesg | grep -E "fb1|ads7846"
cat /sys/class/graphics/fb1/name

# Fill the screen red (RGB565 little endian 0xF800)
python3 -c "open('/dev/fb1','wb').write(b'\x00\xf8' * (480 * 320))"

# Touch events
sudo cat /dev/input/event0 | xxd | head
```

The touch device node can change between boots if other input devices are added.
Look it up by name (`/proc/bus/input/devices`, "ADS7846 Touchscreen") or via
`/dev/input/by-path/`. The apps in `apps/` do this.

## Touch calibration

Four targets were drawn at (24,24), (456,24), (456,296), (24,296) and tapped in
that order. With the final overlay the device reports:

- `ABS_X`: 0 to 3724, increasing left to right
- `ABS_Y`: 346 to 3864, increasing top to bottom
- pressure (`ABS_PRESSURE`) and `BTN_TOUCH` (code 330)

Linear fit of the measured corners:

```
ABS_X = 7.74  * x_px - 36        x_px = (ABS_X + 36)  / 7.74
ABS_Y = 11.14 * y_px + 280       y_px = (ABS_Y - 280) / 11.14
```

Plain range scaling (`x = ABS_X / 3724 * 480`, `y = (ABS_Y - 346) / 3518 * 320`)
is off by about 5-6 px near the left and top edges and accurate on the right and
bottom. For pixel-exact input, apply the fit above in the application, or pass
the screen-edge values to LVGL as the apps in `apps/` do. Resistive touch also
drifts a little with pressure and temperature, so re-run the calibration if it
feels off.

`calib.py` draws a crosshair per corner on `/dev/fb1`, averages the raw taps and
prints JSON. It reads `/dev/input/event0` and writes `/dev/fb1` directly, so run
it on the Pi with `sudo`, with the clock service stopped.

## Troubleshooting log

- First boot with `dtoverlay=piscreen,speed=16000000,rotate=90`: the driver
  loaded without errors, but the screen stayed black and touch was silent.
  Lowering the SPI speed to 8 MHz changed nothing.
- The cause was the board not making full contact on the 40-pin header. After
  reseating it and a full power cycle, display and touch both worked. If the
  screen is white or black and `dmesg` shows a clean driver load, reseat the
  board first.
- The speed went back to 16 MHz and the custom touch overlay was built.

## Apps

`apps/` holds the LVGL apps (digital clock, KOMARO sensor display, hello-touch
demo), the shared `suzie_platform` library and a Python drawing demo. They draw
to `/dev/fb1` and read the touch device by name, so nothing needs to be disabled
on the console. The clock can start at boot through a systemd service. Build and
install instructions are in `apps/README.md` and `apps/clock/README.md`.

## Undo

```sh
sudo cp /boot/config.txt.bak-suziepi /boot/config.txt
sudo cp /boot/cmdline.txt.bak-suziepi /boot/cmdline.txt
sudo rm /boot/overlays/piscreen-suzie.dtbo
sudo reboot
```

If the clock service is installed, also run `apps/clock/scripts/service.sh
uninstall`.
