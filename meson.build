project( 'nixd'
       , ['c', 'cpp']
       , default_options : ['cpp_std=gnu++20']
       )


add_project_arguments('-DSYSTEM="@0@"'.format(get_option('system')), language: 'cpp')

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
