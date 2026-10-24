suzie_platform_lib = static_library('suzie_platform', 'suzie_platform.c',
  dependencies : lvgl_dep)

suzie_platform_dep = declare_dependency(
  link_with : suzie_platform_lib,
  include_directories : include_directories('.'),
  dependencies : lvgl_dep)
