nixd_attrset_eval = executable(
  'nixd-attrset-eval',
  'nixd-attrset-eval.cpp',
  include_directories : common_inc,
  link_with : nixd_common_lib,
  dependencies : common_deps,
  install : true,
  install_dir : get_option('libexecdir'),
)

meson.override_find_program('nixd-attrset-eval', nixd_attrset_eval)
