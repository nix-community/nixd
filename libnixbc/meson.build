libnixbc_deps = [ libbc ]
libnixbc_inc = include_directories('include')

libnixbc_lib = library(
    'nixbc',
    'src/Origin.cpp',
    include_directories: libnixbc_inc,
    dependencies: libnixbc_deps,
)

libnixbc = declare_dependency(
    include_directories: libnixbc_inc,
    link_with: libnixbc_lib,
    dependencies: libnixbc_deps,
)

install_subdir('include/nixbc', install_dir: 'include')
