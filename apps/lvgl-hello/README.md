# lvgl-hello

Small LVGL 9 app for the SuziePi 3.5" screen: a title, a counter button, a
slider, and a live touch coordinate readout. It renders to `/dev/fb1` and reads
the XPT2046 touch controller through evdev, using the calibration from
`../../calibration.txt`.

## Build (on the Pi)

Needs `gcc`, `cmake`, `make`, `git` and network access (CMake downloads LVGL
v9.2.2 on the first configure).

```sh
cd apps/lvgl-hello
cmake -S . -B build
cmake --build build -j2
```

The Pi 3 A+ has 512 MB of RAM, so keep the job count low (`-j2`).

## Run

```sh
sudo ./build/hello
sudo ./build/hello -f /dev/fb1 -t /dev/input/event0   # explicit devices
```

Without `-t` the touch node is found by name ("ADS7846 Touchscreen") in
`/proc/bus/input/devices`. Stop with the on-screen **Exit** button or Ctrl-C.

## Notes

- LVGL is configured with compile definitions in `CMakeLists.txt` (no
  `lv_conf.h`): 16-bit color, libc malloc, fbdev and evdev drivers, Montserrat
  14 and 20 fonts. Add `LV_...` definitions there to enable more features.
- If touch is offset, adjust `CAL_MIN_X/MAX_X/MIN_Y/MAX_Y` in `main.c`
  (raw values at the screen edges) or re-run `../../calib.py`.
- The screen rotation (90) is handled by the kernel overlay, so LVGL sees a
  plain 480x320 framebuffer.
