project( 'nixf'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: '2.7.0'
       )

nix_expr = dependency('nix-expr')
nlohmann_json = dependency('nlohmann_json')
boost = dependency('boost')
gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

pkgconfig = import('pkgconfig')


subdir('src')
subdir('test')
subdir('tools')
