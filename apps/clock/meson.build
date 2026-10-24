project('clock', 'c',
  version : '0.1.0',
  default_options : ['c_std=gnu11', 'warning_level=2', 'buildtype=release'])

# LVGL is configured with compile definitions only (no lv_conf.h). Global
# arguments are used so that the lvgl subproject sees them too.
add_global_arguments(
  '-DLV_CONF_SKIP=1',
  '-DLV_COLOR_DEPTH=16',          # fb1 is RGB565
  '-DLV_USE_STDLIB_MALLOC=1',     # LV_STDLIB_CLIB: libc malloc, no fixed pool
  '-DLV_USE_LINUX_FBDEV=1',
  '-DLV_USE_EVDEV=1',
  '-DLV_FONT_MONTSERRAT_14=1',
  '-DLV_FONT_MONTSERRAT_20=1',
  '-DLV_USE_TINY_TTF=1',          # scalable digits from an embedded font
  language : 'c')

lvgl_dep = dependency('lvgl', fallback : ['lvgl', 'lvgl_dep'])

clock = executable('clock', 'src/main.c', 'src/digits_font.c',
  dependencies : lvgl_dep,
  install : true)

# `meson compile -C <builddir> deploy` copies the binary to the Pi.
run_target('deploy',
  command : [files('scripts/deploy.sh'), clock])
