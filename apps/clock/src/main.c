/*
 * Digital clock for the SuziePi 3.5" screen.
 * Display, touch and the main loop come from the shared suzie_platform library.
 *
 * Usage: ./clock [-f /dev/fb1] [-t /dev/input/eventN]
 */
#include "suzie_platform.h"

#include <stddef.h>
#include <stdint.h>
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

int main(int argc, char **argv)
{
    if (suzie_init(argc, argv) != 0)
        return 1;
    build_ui();
    suzie_run();
    return 0;
}
