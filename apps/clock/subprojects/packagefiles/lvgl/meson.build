# meson wrapper for LVGL (upstream only ships CMake). Compiles every C file
# under src/; features that are disabled by LV_USE_* compile to nothing.
# Configuration comes from global compile arguments set by the main project.
project('lvgl', 'c', version : '9.2.2')

py = find_program('python3')
globbed = run_command(py, files('glob_sources.py'), meson.current_source_dir(),
                      check : true).stdout().strip().split('\n')

lvgl_lib = static_library('lvgl', files(globbed),
  include_directories : include_directories('.'),
  c_args : ['-w'])

lvgl_dep = declare_dependency(
  link_with : lvgl_lib,
  include_directories : include_directories('.'),
  link_args : ['-lm'])
