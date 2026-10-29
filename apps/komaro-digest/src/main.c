/*
 * komaro-digest: shows the latest InfluxDB sensor values on the SuziePi screen.
 *
 *   Current page  the latest values of the selected sensor
 *   Clock page    HH:MM and the date, like the clock app
 *   Sensors page  the list of sensors (InfluxDB measurements) to pick from
 *   Options page  the C/F and auto-rotate switches
 *
 * The pages are switched with the buttons along the bottom: Home (Current page),
 * Previous and Next. All choices are remembered.
 *
 * Usage: ./komaro-digest [-H host] [-P port] [-d database] [-s sensor] [-u C|F]
 *                        [-r seconds] [-h]
 *                        [-f /dev/fb1] [-t /dev/input/eventN]
 */
#include "suzie_platform.h"

#include "config.h"
#include "model.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define STALE_SECONDS (15 * 60)
#define NAV_BAR_PX 40
#define ROTATE_PERIOD_MS (5 * 1000) /* autorotate: time per sensor */
#define TIME_FONT_PX 140 /* "HH:MM" is about 3 em wide: 140 px -> ~420 px */

extern const uint8_t digits_font_ttf[];
extern const size_t digits_font_ttf_len;

static lv_obj_t *tabview;
static lv_obj_t *sensor_label;
static lv_obj_t *temp_label;
static lv_obj_t *temp_f_label;
static lv_obj_t *humidity_label;
static lv_obj_t *other_label;
static lv_obj_t *status_label;
static lv_obj_t *sensor_list;
static lv_obj_t *unit_switch;
static lv_obj_t *rotate_switch;
static lv_timer_t *rotate_timer;
static lv_obj_t *clock_label;
static lv_obj_t *date_label;

static char unit = 'C';       /* shown large on the Current page */
static int autorotate;        /* 1 = Current page steps through the sensors */
static char sensor_name[INFLUX_NAME_LEN];

enum { TAB_CURRENT, TAB_SENSORS, TAB_CLOCK, TAB_OPTIONS, TAB_COUNT };

static unsigned shown_version = (unsigned)-1;
static influx_names_t shown_names;

static void list_select_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target_obj(e);
    const char *name = lv_list_get_button_text(sensor_list, btn);

    if (!name)
        return;
    snprintf(sensor_name, sizeof(sensor_name), "%s", name);
    model_select(name);
    config_save_state(sensor_name, unit, autorotate);
    lv_timer_reset(rotate_timer); /* the chosen sensor gets a full period */
    lv_tabview_set_active(tabview, TAB_CURRENT, LV_ANIM_ON);
}

static void unit_switch_cb(lv_event_t *e)
{
    (void)e;
    unit = lv_obj_has_state(unit_switch, LV_STATE_CHECKED) ? 'F' : 'C';
    shown_version = (unsigned)-1; /* redraw the values in the new unit */
    config_save_state(sensor_name, unit, autorotate);
}

static void rotate_switch_cb(lv_event_t *e)
{
    (void)e;
    autorotate = lv_obj_has_state(rotate_switch, LV_STATE_CHECKED);
    lv_timer_reset(rotate_timer);
    config_save_state(sensor_name, unit, autorotate);
}

/* Every ROTATE_PERIOD_MS, while the Current page is open, move on to the next
 * sensor in the list (wrapping around). */
static void rotate_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!autorotate || lv_tabview_get_tab_active(tabview) != TAB_CURRENT)
        return;
    if (shown_names.count < 2)
        return;

    int cur = -1;
    for (int i = 0; i < shown_names.count; i++) {
        if (strcmp(shown_names.name[i], sensor_name) == 0) {
            cur = i;
            break;
        }
    }
    const char *next = shown_names.name[(cur + 1) % shown_names.count];

    snprintf(sensor_name, sizeof(sensor_name), "%s", next);
    model_select(next);
    config_save_state(sensor_name, unit, autorotate);
}

static void tab_changed_cb(lv_event_t *e);

static void nav_cb(lv_event_t *e)
{
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    int tab = TAB_CURRENT;

    if (step != 0) {
        tab = ((int)lv_tabview_get_tab_active(tabview) + step + TAB_COUNT) % TAB_COUNT;
    }
    lv_tabview_set_active(tabview, (uint32_t)tab, LV_ANIM_OFF);
    tab_changed_cb(NULL);
}

static lv_obj_t *make_nav_button(lv_obj_t *bar, const char *text, int step)
{
    lv_obj_t *btn = lv_button_create(bar);
    lv_obj_set_height(btn, lv_pct(100));
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_add_event_cb(btn, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)step);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return btn;
}

static void tab_changed_cb(lv_event_t *e)
{
    (void)e;
    if (lv_tabview_get_tab_active(tabview) == TAB_SENSORS)
        model_refresh(); /* fresh list of sensors when the page is opened */
}

static void rebuild_list(const model_snapshot_t *s)
{
    lv_obj_clean(sensor_list);
    for (int i = 0; i < s->names.count; i++) {
        lv_obj_t *btn = lv_list_add_button(sensor_list, LV_SYMBOL_FILE, s->names.name[i]);
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), LV_STATE_CHECKED);
        lv_obj_add_event_cb(btn, list_select_cb, LV_EVENT_CLICKED, NULL);
    }
    shown_names = s->names;
}

static void mark_selected(const model_snapshot_t *s)
{
    uint32_t n = lv_obj_get_child_count(sensor_list);

    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(sensor_list, (int32_t)i);
        const char *text = lv_list_get_button_text(sensor_list, child);
        if (text && strcmp(text, s->selected) == 0)
            lv_obj_add_state(child, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(child, LV_STATE_CHECKED);
    }
}

static void update_status(const model_snapshot_t *s)
{
    char buf[128];
    lv_color_t color = lv_color_hex(0x909090);

    if (s->error[0]) {
        snprintf(buf, sizeof(buf), "%s", s->error);
        color = lv_palette_main(LV_PALETTE_RED);
    } else if (s->no_data) {
        snprintf(buf, sizeof(buf), "No data for %s", s->selected);
        color = lv_palette_main(LV_PALETTE_ORANGE);
    } else if (s->have_reading && s->reading.time != (time_t)-1 && s->reading.time != 0) {
        time_t now = time(NULL);
        long age = (long)(now - s->reading.time);
        struct tm tm;
        char hhmm[16];

        localtime_r(&s->reading.time, &tm);
        strftime(hhmm, sizeof(hhmm), "%H:%M", &tm);
        if (age < 0)
            age = 0;
        if (age < 90)
            snprintf(buf, sizeof(buf), "updated %s (just now)", hhmm);
        else if (age < 3600)
            snprintf(buf, sizeof(buf), "updated %s (%ld min ago)", hhmm, age / 60);
        else
            snprintf(buf, sizeof(buf), "updated %s (%ld h %ld min ago)", hhmm, age / 3600,
                     (age % 3600) / 60);
        if (age > STALE_SECONDS)
            color = lv_palette_main(LV_PALETTE_ORANGE);
    } else {
        snprintf(buf, sizeof(buf), "waiting for data...");
    }
    lv_label_set_text(status_label, buf);
    lv_obj_set_style_text_color(status_label, color, 0);
}

static void update_values(const model_snapshot_t *s)
{
    char buf[160];
    const influx_reading_t *r = &s->reading;

    lv_label_set_text(sensor_label, s->selected[0] ? s->selected : "-");
    if (!s->have_reading) {
        lv_label_set_text(temp_label, "--");
        lv_label_set_text(temp_f_label, "");
        lv_label_set_text(humidity_label, "");
        lv_label_set_text(other_label, "");
        return;
    }

    if (influx_has_field(r, "temperature_c")) {
        double c = influx_field(r, "temperature_c", 0);
        double f = influx_has_field(r, "temperature_f") ? influx_field(r, "temperature_f", 0)
                                                        : c * 9.0 / 5.0 + 32.0;
        /* The chosen unit big, the other one small underneath. */
        snprintf(buf, sizeof(buf), "%.1f " "\xC2\xB0" "%c", unit == 'F' ? f : c, unit);
        lv_label_set_text(temp_label, buf);
        snprintf(buf, sizeof(buf), "%.1f " "\xC2\xB0" "%c", unit == 'F' ? c : f,
                 unit == 'F' ? 'C' : 'F');
        lv_label_set_text(temp_f_label, buf);
        lv_label_set_text(other_label, "");
    } else {
        /* Not a temperature sensor: list the first few fields instead. */
        lv_label_set_text(temp_label, "");
        lv_label_set_text(temp_f_label, "");
        buf[0] = '\0';
        for (int i = 0; i < r->nfields && i < 5; i++) {
            size_t used = strlen(buf);
            snprintf(buf + used, sizeof(buf) - used, "%s%s: %g", i ? "\n" : "", r->fields[i].name,
                     r->fields[i].value);
        }
        lv_label_set_text(other_label, buf);
    }

    if (influx_has_field(r, "humidity")) {
        snprintf(buf, sizeof(buf), "%.0f %%", influx_field(r, "humidity", 0));
        lv_label_set_text(humidity_label, buf);
    } else {
        lv_label_set_text(humidity_label, "");
    }
}

static void poll_cb(lv_timer_t *timer)
{
    model_snapshot_t s;
    (void)timer;

    model_snapshot(&s);
    snprintf(sensor_name, sizeof(sensor_name), "%s", s.selected);
    if (s.version != shown_version) {
        shown_version = s.version;
        if (s.have_names && memcmp(&s.names, &shown_names, sizeof(s.names)) != 0)
            rebuild_list(&s);
        mark_selected(&s);
        update_values(&s);
    }
    update_status(&s); /* the age of the reading changes with time */
}

static void clock_cb(lv_timer_t *timer)
{
    static int last_min = -1;
    time_t now = time(NULL);
    struct tm tm;
    char buf[48];
    (void)timer;

    localtime_r(&now, &tm);
    if (tm.tm_min == last_min)
        return;
    last_min = tm.tm_min;
    strftime(buf, sizeof(buf), "%H:%M", &tm);
    lv_label_set_text(clock_label, buf);
    strftime(buf, sizeof(buf), "%A, %B %d %Y", &tm);
    lv_label_set_text(date_label, buf);
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, "");
    return l;
}

static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    tabview = lv_tabview_create(scr);
    lv_obj_set_size(tabview, lv_pct(100), lv_display_get_vertical_resolution(NULL) - NAV_BAR_PX);
    lv_obj_align(tabview, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_tabview_set_tab_bar_size(tabview, 0); /* pages are switched by the nav bar */
    lv_obj_add_flag(lv_tabview_get_tab_bar(tabview), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(tabview, lv_color_hex(0x101820), 0);
    lv_obj_add_event_cb(tabview, tab_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *cur = lv_tabview_add_tab(tabview, "Current");
    lv_obj_t *sens = lv_tabview_add_tab(tabview, "Sensors");
    lv_obj_t *clk = lv_tabview_add_tab(tabview, "Clock");
    lv_obj_t *opt = lv_tabview_add_tab(tabview, "Options");
    lv_obj_set_scrollbar_mode(cur, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(cur, LV_OBJ_FLAG_SCROLLABLE);

    /* Current page */
    lv_color_t white = lv_color_white();
    sensor_label = make_label(cur, &lv_font_montserrat_20, lv_color_hex(0x80C0FF));
    lv_obj_align(sensor_label, LV_ALIGN_TOP_LEFT, 0, 4);

    temp_label = make_label(cur, &lv_font_montserrat_48, lv_color_hex(0x40FF80));
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, -45);
    temp_f_label = make_label(cur, &lv_font_montserrat_20, lv_color_hex(0x90D0A0));
    lv_obj_align(temp_f_label, LV_ALIGN_CENTER, 0, -5);
    humidity_label = make_label(cur, &lv_font_montserrat_48, lv_color_hex(0x60B0FF));
    lv_obj_align(humidity_label, LV_ALIGN_CENTER, 0, 45);
    other_label = make_label(cur, &lv_font_montserrat_20, white);
    lv_obj_align(other_label, LV_ALIGN_CENTER, 0, 0);

    status_label = make_label(cur, &lv_font_montserrat_14, lv_color_hex(0x909090));
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Clock page */
    lv_obj_set_style_pad_all(clk, 0, 0);
    lv_obj_remove_flag(clk, LV_OBJ_FLAG_SCROLLABLE);
    clock_label = make_label(clk, &lv_font_montserrat_48, lv_color_hex(0x40FF80));
    lv_font_t *time_font = lv_tiny_ttf_create_data(digits_font_ttf, digits_font_ttf_len,
                                                   TIME_FONT_PX);
    if (time_font)
        lv_obj_set_style_text_font(clock_label, time_font, 0);
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, -10);
    date_label = make_label(clk, &lv_font_montserrat_20, lv_color_hex(0xA0A0A0));
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -6);

    /* Sensors page: the Exit button covers the top-right corner, so the list
     * starts below it. */
    lv_obj_set_style_pad_all(sens, 0, 0);
    lv_obj_t *title = make_label(sens, &lv_font_montserrat_20, white);
    lv_label_set_text(title, "Select a sensor");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 8);

    sensor_list = lv_list_create(sens);
    lv_obj_set_size(sensor_list, lv_pct(100), 190);
    lv_obj_align(sensor_list, LV_ALIGN_TOP_LEFT, 0, 44);

    /* Options page */
    lv_obj_set_style_pad_all(opt, 0, 0);
    lv_obj_t *opt_title = make_label(opt, &lv_font_montserrat_20, white);
    lv_label_set_text(opt_title, "Options");
    lv_obj_align(opt_title, LV_ALIGN_TOP_LEFT, 8, 8);

    lv_obj_t *u_lbl = make_label(opt, &lv_font_montserrat_20, white);
    lv_label_set_text(u_lbl, "Temperature unit");
    lv_obj_align(u_lbl, LV_ALIGN_TOP_LEFT, 8, 64);
    /* C [switch] F */
    lv_obj_t *c_lbl = make_label(opt, &lv_font_montserrat_20, white);
    lv_label_set_text(c_lbl, "C");
    lv_obj_align(c_lbl, LV_ALIGN_TOP_LEFT, 280, 64);
    unit_switch = lv_switch_create(opt);
    lv_obj_set_size(unit_switch, 56, 28);
    lv_obj_align(unit_switch, LV_ALIGN_TOP_LEFT, 308, 62);
    if (unit == 'F')
        lv_obj_add_state(unit_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(unit_switch, unit_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *f_lbl = make_label(opt, &lv_font_montserrat_20, white);
    lv_label_set_text(f_lbl, "F");
    lv_obj_align(f_lbl, LV_ALIGN_TOP_LEFT, 374, 64);

    lv_obj_t *r_lbl = make_label(opt, &lv_font_montserrat_20, white);
    lv_label_set_text(r_lbl, "Auto rotate (every 5 s)");
    lv_obj_align(r_lbl, LV_ALIGN_TOP_LEFT, 8, 124);
    rotate_switch = lv_switch_create(opt);
    lv_obj_set_size(rotate_switch, 56, 28);
    lv_obj_align(rotate_switch, LV_ALIGN_TOP_LEFT, 308, 122);
    if (autorotate)
        lv_obj_add_state(rotate_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(rotate_switch, rotate_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Navigation bar along the bottom */
    lv_obj_t *nav = lv_obj_create(scr);
    lv_obj_set_size(nav, lv_pct(100), NAV_BAR_PX);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(nav, 2, 0);
    lv_obj_set_style_pad_column(nav, 4, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0x101820), 0);
    lv_obj_remove_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    make_nav_button(nav, LV_SYMBOL_HOME " Home", 0);
    make_nav_button(nav, LV_SYMBOL_LEFT " Previous", -1);
    make_nav_button(nav, "Next " LV_SYMBOL_RIGHT, 1);

    suzie_exit_button(scr);
    rotate_timer = lv_timer_create(rotate_cb, ROTATE_PERIOD_MS, NULL);
    lv_timer_create(poll_cb, 500, NULL);
    lv_timer_create(clock_cb, 500, NULL);
    clock_cb(NULL);
}

static void usage(const char *prog)
{
    printf("Usage: %s [options]\n"
           "\n"
           "Show the latest InfluxDB sensor values on the SuziePi screen.\n"
           "\n"
           "  -H host     InfluxDB host (default silvana.home)\n"
           "  -P port     InfluxDB HTTP port (default 8086)\n"
           "  -d name     database (default komaro)\n"
           "  -s name     sensor (measurement) to show first (default: last choice)\n"
           "  -u C|F      unit of the large temperature (default: last choice, else C)\n"
           "  -r seconds  seconds between re-reads of the data, min 5 (default 180)\n"
           "  -f device   framebuffer device (default /dev/fb1)\n"
           "  -t device   touch device (default: ADS7846 found by name)\n"
           "  -z N        SDL window zoom 1..8, default 2 (desktop build only)\n"
           "  -T title    SDL window title, default title= in config (desktop build only)\n"
           "  -h          show this help\n"
           "\n"
           "Settings files in ~/.config/komaro-digest/: config (host=, port=, db=,\n"
           "refresh=, zoom=, title=) and state (sensor=, unit=, autorotate=, written by the app).\n",
           prog);
}

int main(int argc, char **argv)
{
    app_config_t cfg;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    config_load(&cfg, argc, argv);
    suzie_set_default_zoom(cfg.zoom); /* -z on the command line still wins */
    suzie_set_default_title(cfg.title[0] ? cfg.title : NULL);
    if (suzie_init(argc, argv) != 0)
        return 1;

    lv_display_t *disp = lv_display_get_default();
    lv_display_set_theme(disp, lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                                     lv_palette_main(LV_PALETTE_CYAN), true,
                                                     LV_FONT_DEFAULT));
    unit = cfg.unit;
    autorotate = cfg.autorotate;
    snprintf(sensor_name, sizeof(sensor_name), "%s", cfg.sensor);
    build_ui();
    model_start(&cfg.conn, cfg.sensor, cfg.refresh_s);
    suzie_run();
    return 0;
}
