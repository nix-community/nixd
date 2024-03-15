libbc_inc = include_directories('include')

libbc_lib = library(
    'bc',
    'src/Read.cpp',
    'src/Write.cpp',
    include_directories: libbc_inc,
    install: true
)

libbc = declare_dependency(
    include_directories: libbc_inc,
    link_with: libbc_lib,
)

install_subdir('include/bc', install_dir: 'include')
