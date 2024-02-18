libnixbc_inc = include_directories('include')

libnixbc_lib = library(
    'nixbc',
    'src/Deserialize.cpp',
    'src/Serialize.cpp',
    include_directories: libnixbc_inc,
)

libnixbc = declare_dependency(
    include_directories: libnixbc_inc,
    link_with: libnixbc_lib,
)

install_subdir('include/nixbc', install_dir: 'include')
