thread_dep = dependency('threads')

digest_lib = static_library('digest',
  'src/json.c', 'src/net.c', 'src/influx.c')

executable('komaro-digest',
  'src/main.c', 'src/model.c', 'src/config.c',
  '../clock/src/digits_font.c', # the clock's HH:MM font
  include_directories : include_directories('src'),
  link_with : digest_lib,
  dependencies : [suzie_platform_dep, thread_dep],
  install : true)

# Unit tests run only in a native build (a cross build cannot run them here).
if not meson.is_cross_build()
  test_parse = executable('test_parse', 'tests/test_parse.c',
    include_directories : include_directories('src'),
    link_with : digest_lib)
  test('parse', test_parse)
endif
