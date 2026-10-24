/*
 * Digital clock for the SuziePi 3.5" screen (LVGL, fbdev + evdev).
 *
 * Output: /dev/fb1 (480x320 RGB565)
 * Input:  evdev node named "ADS7846 Touchscreen"
 *
 * Usage: sudo ./clock [-f /dev/fb1] [-t /dev/input/eventN]
 */
#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_FB "/dev/fb1"
#define TIME_FONT_PX 140 /* "HH:MM" is about 3 em wide: 140 px -> ~420 px on 480 */

/* Raw ABS_X/ABS_Y values at the screen edges (see ../../calibration.txt). */
#define CAL_MIN_X (-36)
#define CAL_MAX_X 3681
#define CAL_MIN_Y 280
#define CAL_MAX_Y 3844

static lv_obj_t *time_label;
static lv_obj_t *date_label;
static volatile int quit_requested;

extern const uint8_t digits_font_ttf[];
extern const size_t digits_font_ttf_len;

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

static void update_cb(lv_timer_t *timer)
{
    static int last_min = -1;
    time_t now = time(NULL);
    (void)timer;

    struct tm tm;
    char buf[48];
    localtime_r(&now, &tm);
    if (tm.tm_min == last_min)
        return;
    last_min = tm.tm_min;

    strftime(buf, sizeof(buf), "%H:%M", &tm);
    lv_label_set_text(time_label, buf);
    strftime(buf, sizeof(buf), "%A, %B %d %Y", &tm);
    lv_label_set_text(date_label, buf);
}

static void exit_event_cb(lv_event_t *e)
{
    (void)e;
    quit_requested = 1;
}

static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    time_label = lv_label_create(scr);
    lv_font_t *time_font = lv_tiny_ttf_create_data(digits_font_ttf, digits_font_ttf_len, TIME_FONT_PX);
    lv_label_set_text(time_label, "--:--");
    if (time_font)
        lv_obj_set_style_text_font(time_label, time_font, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x40FF80), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -10);

    date_label = lv_label_create(scr);
    lv_label_set_text(date_label, "");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xA0A0A0), 0);
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_t *exit_btn = lv_button_create(scr);
    lv_obj_set_size(exit_btn, 72, 36);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xB03030), 0);
    lv_obj_add_event_cb(exit_btn, exit_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, "Exit");
    lv_obj_center(exit_label);

    lv_timer_create(update_cb, 500, NULL);
    update_cb(NULL);
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
