project( 'nixt'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: 'nightly'
       )

boost = dependency('boost')
gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

pkgconfig = import('pkgconfig')


nix_main = dependency('nix-main')
nix_expr = dependency('nix-expr')
nix_cmd = dependency('nix-cmd')

subdir('lib')
subdir('test')
