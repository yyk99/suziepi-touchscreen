# apps

LVGL apps for the SuziePi 3.5" screen, built as one meson project that is
cross-compiled for the Pi 3 (static 32-bit ARM binaries). See `../README.md` for
the hardware setup.

## Contents

```
┌───────────────────┬─────────────────────────────────────────────────────────────────────────────┐
│ Directory         │ What                                                                        │
├───────────────────┼─────────────────────────────────────────────────────────────────────────────┤
│ platform/         │ suzie_platform library: display, touch, calibration, main loop, Exit button │
│ clock/            │ Digital clock; runs at boot via systemd (see its README)                    │
│ lvgl-hello-meson/ │ Hello-touch demo built on platform/                                         │
│ lvgl-hello/       │ The same demo built with CMake, with its own copy of the glue (legacy)      │
│ touchdraw.py      │ Python drawing demo, no build needed                                        │
│ cross/pi3.ini     │ Meson cross file for the Pi 3 (static, armhf)                               │
│ subprojects/      │ LVGL v9.2.2 wrap and its meson build files                                  │
└───────────────────┴─────────────────────────────────────────────────────────────────────────────┘
```

## Build

```sh
sudo apt install meson ninja-build gcc-arm-linux-gnueabihf
cd apps
meson setup build-arm --cross-file cross/pi3.ini
meson compile -C build-arm
# -> build-arm/clock/clock, build-arm/lvgl-hello-meson/hello
```

For a quick native compile check: `meson setup build-native && meson compile -C
build-native`. The first configure downloads LVGL (needs network and `git`).
Copy a binary to the Pi and run it there; stop the clock service first if it is
enabled (`clock/scripts/service.sh stop`), because only one app can use the
screen at a time.

## Writing a new app

```c
#include "suzie_platform.h"

int main(int argc, char **argv)
{
    if (suzie_init(argc, argv) != 0)   /* -f <fbdev>, -t <evdev> */
        return 1;
    lv_obj_t *scr = lv_screen_active();
    /* ... build the UI ... */
    suzie_exit_button(scr);            /* optional */
    suzie_run();                       /* until Exit, SIGINT, SIGTERM */
    return 0;
}
```

Create `apps/<name>/meson.build` with `executable('<name>', 'main.c',
dependencies : suzie_platform_dep)` and add `subdir('<name>')` to
`apps/meson.build`.

## Design notes

- LVGL once: the LVGL options (RGB565, libc malloc, fbdev and evdev drivers,
  Tiny TTF, Montserrat 14 and 20) are global compile arguments in `meson.build`,
  so every app shares one LVGL build. There is no `lv_conf.h`.
- LVGL under meson: upstream has no `meson.build`, so
  `subprojects/packagefiles/lvgl/` provides one (applied by
  `subprojects/lvgl.wrap`). It compiles all `src/**/*.c` as C only, so no C++
  compiler is needed.
- Static linking: `cross/pi3.ini` links with `-static`. The host glibc (2.39) is
  newer than the Pi's (2.36); a dynamic binary can reference symbols the Pi
  lacks. Static binaries avoid that and need no sysroot.
- CPU flags: `-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard` targets the Pi
  3 family running 32-bit Raspberry Pi OS. It will not run on a Pi Zero or Pi 1
  (ARMv6). Remove those flags for a generic ARMv7 build.
- Why a source library: LVGL's compile definitions change its code and struct
  layouts, so the platform library is compiled together with the LVGL of the
  same build instead of being shipped prebuilt.
