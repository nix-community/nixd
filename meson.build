project( 'nixd'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       )


add_project_arguments('-DSYSTEM="@0@"'.format(get_option('system')), language: 'cpp')

nix_expr = dependency('nix-expr')
nix_cmd = dependency('nix-cmd')
nix_store = dependency('nix-store')
llvm = dependency('llvm')
boost = dependency('boost')


subdir('lib')

executable('nixd', ['tools/nixd.cpp']
, dependencies: [ boost
                , llvm
                , nixd_lsp_server
                , nixd_server
                ]
)
