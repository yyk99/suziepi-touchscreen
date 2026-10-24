cmake_minimum_required(VERSION 3.16)
project(lvgl_hello C)

set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# LVGL is configured entirely through compile definitions (no lv_conf.h).
# Applied to every target created below, including lvgl itself.
add_compile_definitions(
  LV_CONF_SKIP=1
  LV_COLOR_DEPTH=16            # fb1 is RGB565
  LV_USE_STDLIB_MALLOC=1       # LV_STDLIB_CLIB: use libc malloc, no fixed pool
  LV_USE_LINUX_FBDEV=1
  LV_USE_EVDEV=1
  LV_FONT_MONTSERRAT_14=1
  LV_FONT_MONTSERRAT_20=1
)

include(FetchContent)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG        v9.2.2
  GIT_SHALLOW    TRUE)
# Skip the examples, demos and the bundled C++ ThorVG renderer: we use none of
# them, and they are ~370 of the ~690 files LVGL would otherwise compile.
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON)
set(LV_CONF_BUILD_DISABLE_DEMOS ON)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON)
FetchContent_MakeAvailable(lvgl)

add_executable(hello main.c)
target_link_libraries(hello PRIVATE lvgl)
