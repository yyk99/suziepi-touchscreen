/*
 * suzie_platform - shared glue for LVGL apps on the SuziePi 3.5" screen.
 *
 * Output: fbtft framebuffer (/dev/fb1, 480x320 RGB565)
 * Input:  XPT2046 touch controller ("ADS7846 Touchscreen") via evdev,
 *         calibrated for this panel (see ../../calibration.txt)
 *
 * Native builds with the SDL2 development files draw into a 480x320 window
 * instead and use the mouse as the touch screen (no fbdev/evdev, -f/-t are
 * ignored). "-z N" (1..8, default 2) magnifies the window N times; it exists
 * only in SDL builds.
 *
 * Typical use:
 *     if (suzie_init(argc, argv) != 0) return 1;
 *     ... build the UI on lv_screen_active(), e.g. suzie_exit_button() ...
 *     suzie_run();
 */
#ifndef SUZIE_PLATFORM_H
#define SUZIE_PLATFORM_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise LVGL, the framebuffer display and the touch input.
 * Understands "-f <fbdev>" and "-t <evdev>" on the command line (SDL builds:
 * "-z <zoom>" instead); the touch node is found by name when -t is not given. Also makes SIGINT/SIGTERM end
 * suzie_run() cleanly. Returns 0 on success, -1 (after printing why) on error. */
int suzie_init(int argc, char **argv);

/* SDL builds: the window zoom (1..8) used when "-z" is not given, e.g. from a
 * config file. Call before suzie_init(). Out of range values are ignored; has
 * no effect without SDL. */
void suzie_set_default_zoom(int zoom);

/* Run the LVGL main loop until suzie_request_quit() or SIGINT/SIGTERM, then
 * blank the screen so the last frame does not stay on the panel. */
void suzie_run(void);

/* Ask suzie_run() to return. */
void suzie_request_quit(void);

/* Create a red "Exit" button in the top-right corner of parent that calls
 * suzie_request_quit() when clicked. */
lv_obj_t *suzie_exit_button(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif
