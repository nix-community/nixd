project('libnixt',
        ['c', 'cpp'],
        default_options : ['cpp_std=gnu++20'],
        version: 'nightly'
)

nixf_inc = include_directories('include')

gtest = dependency('gtest')
gtest_main = dependency('gtest_main')
fmt = dependency('fmt')
pkgconfig = import('pkgconfig')
lit = find_program('lit', required: false)

subdir('lib')
subdir('test')

install_subdir('include', install_dir : 'include/nixf')

subdir('tools')
