libnixfParse_deps = [ nix_expr, nix_main, nix_cmd, boost, nixfBasic, nixfSyntax, nixfLex ]

libnixfParse_inc = nixf_inc

libnixfParse = library('nixfParse',
                       'Parser.cpp',
                       include_directories: libnixfParse_inc,
                       dependencies: libnixfParse_deps,
                       install: true
)

pkgconfig.generate(name: 'libnixfParse',
                   version: 'nightly',
                   description: 'libnixfParse',
                   libraries: libnixfParse
)

nixfParse = declare_dependency(include_directories: libnixfParse_inc,
                               link_with: libnixfParse,
                               dependencies: libnixfParse_deps
)
