libnixfLex_deps = [ nixfBasic, nixfSyntax ]

libnixfLex_inc = nixf_inc

libnixfLex = library('nixfLex',
                     'Lexer.cpp',
                     include_directories: libnixfLex_inc,
                     dependencies: libnixfLex_deps,
                     install: true
)

pkgconfig.generate(name: 'libnixfLex',
                   version: 'nightly',
                   description: 'libnixfLex',
                   libraries: libnixfLex
)

nixfLex = declare_dependency(include_directories: libnixfLex_inc,
                             link_with: libnixfLex,
                             dependencies: libnixfLex_deps
)
