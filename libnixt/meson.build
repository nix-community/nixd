project( 'nixt'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       , version: 'nightly'
       )

boost = dependency('boost')
gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

pkgconfig = import('pkgconfig')

subdir('lib')
subdir('test')
