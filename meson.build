project( 'nixd'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       )

nix_all = [ dependency('nix-expr')
          , dependency('nix-cmd')
          , dependency('nix-store')
          , dependency('nix-main')
          ]

gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

llvm = dependency('llvm')
boost = dependency('boost')


subdir('lib')

executable('nixd', ['tools/nixd.cpp']
, dependencies: [ boost
                , llvm
                , nixd_lsp_server
                , nixd_server
                ] + nix_all
)
