# lvgl-hello-meson

Small LVGL 9 demo (counter button, slider, live touch coordinates, Exit
button) built with **meson** on top of the shared `../platform` library. It is
the meson counterpart of the CMake app in `../lvgl-hello`, which still has its
own copy of the display/touch glue.

Build from `apps/` (see `../README.md`); the binary is
`build-arm/lvgl-hello-meson/hello`. Copy it to the Pi and run it there (stop the
clock service first if it is enabled: `../clock/scripts/service.sh stop`).
