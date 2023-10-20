libnixfBasic_deps = [ nix_expr, nix_main, nix_cmd, boost, fmt ]

libnixfBasic_inc = nixf_inc

libnixfBasic = library('nixfBasic',
                       'Diagnostic.cpp',
                       include_directories: libnixfBasic_inc,
                       dependencies: libnixfBasic_deps,
                       install: true
)

pkgconfig.generate(name: 'libnixfBasic',
                   version: 'nightly',
                   description: 'libnixfBasic',
                   libraries: libnixfBasic
)

nixfBasic = declare_dependency(include_directories: libnixfBasic_inc,
                               link_with: libnixfBasic,
                               dependencies: libnixfBasic_deps
)
