project( 'nixt'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: '2.8.0'
       )

boost = dependency('boost')
gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

pkgconfig = import('pkgconfig')


nix_main = dependency('nix-main')
nix_expr = dependency('nix-expr')
nix_cmd = dependency('nix-cmd')
nix_flake = dependency('nix-flake')

subdir('lib')
subdir('test')
