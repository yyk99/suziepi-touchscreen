/*
 * LVGL hello-touch for the SuziePi 3.5" screen.
 *
 * Output: /dev/fb1 (480x320 RGB565, fbtft ILI9486)
 * Input:  evdev node named "ADS7846 Touchscreen" (XPT2046)
 *
 * Usage: sudo ./hello [-f /dev/fb1] [-t /dev/input/eventN]
 */
#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_FB "/dev/fb1"

/* Raw ABS_X/ABS_Y values that correspond to the screen edges, from the
 * 4-corner calibration (see ../../calibration.txt):
 *   x_px = (ABS_X + 36) / 7.74   -> ABS_X is -36 at x=0 and 3681 at x=480
 *   y_px = (ABS_Y - 280) / 11.14 -> ABS_Y is 280 at y=0 and 3844 at y=320 */
#define CAL_MIN_X (-36)
#define CAL_MAX_X 3681
#define CAL_MIN_Y 280
#define CAL_MAX_Y 3844

static lv_obj_t *coord_label;
static lv_obj_t *count_label;
static int taps;
static volatile int quit_requested;

static uint32_t tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Find the evdev node of the touch controller by its name. */
static int find_touch_device(char *out, size_t len)
{
    FILE *f = fopen("/proc/bus/input/devices", "r");
    if (!f)
        return -1;

    char line[256];
    int is_touch = 0;
    int found = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "N: Name=", 8) == 0) {
            is_touch = strstr(line, "ADS7846") != NULL;
        } else if (is_touch && strncmp(line, "H: Handlers=", 12) == 0) {
            const char *p = strstr(line, "event");
            if (p) {
                snprintf(out, len, "/dev/input/event%d", atoi(p + 5));
                found = 0;
                break;
            }
        }
    }
    fclose(f);
    return found;
}

static void screen_event_cb(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_label_set_text_fmt(coord_label, "x=%d y=%d", (int)p.x, (int)p.y);
    (void)e;
}

static void button_event_cb(lv_event_t *e)
{
    (void)e;
    lv_label_set_text_fmt(count_label, "Taps: %d", ++taps);
}

static void exit_event_cb(lv_event_t *e)
{
    (void)e;
    quit_requested = 1;
}

static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    lv_obj_t *label = lv_event_get_user_data(e);
    lv_label_set_text_fmt(label, "%d%%", (int)lv_slider_get_value(slider));
}

static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_PRESSING, NULL);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Hello SuziePi");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *exit_btn = lv_button_create(scr);
    lv_obj_set_size(exit_btn, 72, 36);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xB03030), 0);
    lv_obj_add_event_cb(exit_btn, exit_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, "Exit");
    lv_obj_center(exit_label);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 200, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, NULL);
    count_label = lv_label_create(btn);
    lv_label_set_text(count_label, "Tap me");
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_20, 0);
    lv_obj_center(count_label);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 320);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 50);
    lv_obj_t *slider_label = lv_label_create(scr);
    lv_label_set_text(slider_label, "0%");
    lv_obj_set_style_text_color(slider_label, lv_color_white(), 0);
    lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, slider_label);

    coord_label = lv_label_create(scr);
    lv_label_set_text(coord_label, "touch the screen");
    lv_obj_set_style_text_color(coord_label, lv_color_hex(0x80c0ff), 0);
    lv_obj_align(coord_label, LV_ALIGN_BOTTOM_LEFT, 8, -6);
}

int main(int argc, char **argv)
{
    const char *fb = DEFAULT_FB;
    char touch[64] = "";

    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "-f") == 0)
            fb = argv[i + 1];
        else if (strcmp(argv[i], "-t") == 0)
            snprintf(touch, sizeof(touch), "%s", argv[i + 1]);
    }
    if (!touch[0] && find_touch_device(touch, sizeof(touch)) != 0) {
        fprintf(stderr, "ADS7846 touchscreen not found (use -t /dev/input/eventN)\n");
        return 1;
    }

    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "cannot create fbdev display\n");
        return 1;
    }
    lv_linux_fbdev_set_file(disp, fb);

    lv_indev_t *indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, touch);
    if (!indev) {
        fprintf(stderr, "cannot open touch device %s\n", touch);
        return 1;
    }
    lv_evdev_set_calibration(indev, CAL_MIN_X, CAL_MIN_Y, CAL_MAX_X, CAL_MAX_Y);

    printf("display %s, touch %s\n", fb, touch);
    build_ui();

    while (!quit_requested) {
        uint32_t wait = lv_timer_handler();
        if (wait > 30)
            wait = 30;
        usleep(wait * 1000);
    }

    /* Blank the screen so the last frame does not stay on the panel. */
    lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    for (int i = 0; i < 5; i++) {
        lv_timer_handler();
        usleep(20 * 1000);
    }
    return 0;
}
