libnixt_deps = [ nix_expr, nix_main, nix_cmd, boost ]

libnixd_inc = include_directories('../include')

libnixt = library('nixt',
                  'Displacement.cpp',
                  'Flake.cpp',
                  'HookExpr.cpp',
                  'Kinds.cpp',
                  'ParentMap.cpp',
                  'Value.cpp',
                  include_directories: libnixd_inc,
                  dependencies: libnixt_deps,
                  install: true
)

pkgconfig.generate(name: 'nixt',
                   version: 'nightly',
                   description: 'nix compatible layer',
                   subdirs: [ 'nixt' ],
                   libraries: libnixt,
                   requires: [ 'nix-expr', 'nix-main', 'nix-cmd' ]
)


nixt = declare_dependency(include_directories: libnixd_inc,
                          dependencies: libnixt_deps,
                          link_with: libnixt
)

install_subdir('../include/nixt', install_dir: 'include')
