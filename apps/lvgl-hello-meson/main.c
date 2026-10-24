/*
 * LVGL hello-touch for the SuziePi 3.5" screen.
 * Display, touch and the main loop come from the shared suzie_platform library.
 *
 * Usage: ./hello [-f /dev/fb1] [-t /dev/input/eventN]
 */
#include "suzie_platform.h"

static lv_obj_t *coord_label;
static lv_obj_t *count_label;
static int taps;

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

    suzie_exit_button(scr);

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
    if (suzie_init(argc, argv) != 0)
        return 1;
    build_ui();
    suzie_run();
    return 0;
}
