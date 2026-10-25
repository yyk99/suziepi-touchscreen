/*
 * Digital clock for the SuziePi 3.5" screen.
 * Display, touch and the main loop come from the shared suzie_platform library.
 *
 * Usage: ./clock [-f /dev/fb1] [-t /dev/input/eventN] [-h]
 *        ./clock [-z 1..8] [-h]                  (desktop SDL build)
 */
#include "suzie_platform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define TIME_FONT_PX 140 /* "HH:MM" is about 3 em wide: 140 px -> ~420 px on 480 */

static lv_obj_t *time_label;
static lv_obj_t *date_label;

extern const uint8_t digits_font_ttf[];
extern const size_t digits_font_ttf_len;

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

    suzie_exit_button(scr);

    lv_timer_create(update_cb, 500, NULL);
    update_cb(NULL);
}

static void usage(const char *prog)
{
    printf("Usage: %s [options]\n"
           "\n"
           "Digital clock (HH:MM and the date) for the SuziePi 3.5\" screen.\n"
           "\n"
#if LV_USE_SDL
           "  -z N        window zoom 1..8, default 2 (desktop SDL build)\n"
#else
           "  -f device   framebuffer device (default /dev/fb1)\n"
           "  -t device   touch device (default: ADS7846 found by name)\n"
#endif
           "  -h          show this help\n",
           prog);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }
    if (suzie_init(argc, argv) != 0)
        return 1;
    build_ui();
    suzie_run();
    return 0;
}
