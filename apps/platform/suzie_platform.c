#include "suzie_platform.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SCREEN_W 480
#define SCREEN_H 320
#define DEFAULT_ZOOM 2 /* SDL window: integer magnification of the 480x320 screen */
#define MAX_ZOOM 8
#define DEFAULT_FB "/dev/fb1"

/* Raw ABS_X/ABS_Y values at the screen edges, from the 4-corner calibration
 * (see ../../calibration.txt):
 *   x_px = (ABS_X + 36) / 7.74   -> ABS_X is -36 at x=0 and 3681 at x=480
 *   y_px = (ABS_Y - 280) / 11.14 -> ABS_Y is 280 at y=0 and 3844 at y=320 */
#define CAL_MIN_X (-36)
#define CAL_MAX_X 3681
#define CAL_MIN_Y 280
#define CAL_MAX_Y 3844

static volatile sig_atomic_t quit_requested;

static uint32_t tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

#if !LV_USE_SDL
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
#endif

static void on_signal(int sig)
{
    (void)sig;
    quit_requested = 1;
}

static int default_zoom = DEFAULT_ZOOM;

void suzie_set_default_zoom(int zoom)
{
    if (zoom >= 1 && zoom <= MAX_ZOOM)
        default_zoom = zoom;
}

#if LV_USE_SDL
static void display_deleted_cb(lv_event_t *e)
{
    (void)e;
    quit_requested = 1; /* the SDL window was closed */
}

int suzie_init(int argc, char **argv)
{
    int zoom = default_zoom;

    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "-z") == 0) {
            zoom = atoi(argv[i + 1]);
            if (zoom < 1 || zoom > MAX_ZOOM) {
                fprintf(stderr, "-z must be 1..%d\n", MAX_ZOOM);
                return -1;
            }
        }
    }

    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
    if (!disp) {
        fprintf(stderr, "cannot create SDL window\n");
        return -1;
    }
    lv_sdl_window_set_zoom(disp, (uint8_t)zoom);
    lv_display_add_event_cb(disp, display_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_sdl_mouse_create();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    return 0;
}
#else
int suzie_init(int argc, char **argv)
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
        return -1;
    }

    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "cannot create fbdev display\n");
        return -1;
    }
    lv_linux_fbdev_set_file(disp, fb);

    lv_indev_t *indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, touch);
    if (!indev) {
        fprintf(stderr, "cannot open touch device %s\n", touch);
        return -1;
    }
    lv_evdev_set_calibration(indev, CAL_MIN_X, CAL_MIN_Y, CAL_MAX_X, CAL_MAX_Y);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    return 0;
}
#endif

void suzie_request_quit(void)
{
    quit_requested = 1;
}

void suzie_run(void)
{
    while (!quit_requested) {
        uint32_t wait = lv_timer_handler();
        if (wait > 30)
            wait = 30;
        usleep(wait * 1000);
    }

#if !LV_USE_SDL
    /* Blank the screen so the last frame does not stay on the panel. */
    lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    for (int i = 0; i < 5; i++) {
        lv_timer_handler();
        usleep(20 * 1000);
    }
#endif
}

static void exit_event_cb(lv_event_t *e)
{
    (void)e;
    suzie_request_quit();
}

lv_obj_t *suzie_exit_button(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 72, 36);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xB03030), 0);
    lv_obj_add_event_cb(btn, exit_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Exit");
    lv_obj_center(label);
    return btn;
}
