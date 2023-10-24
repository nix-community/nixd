libnixfSyntax_deps = [ nixfBasic ]

libnixfSyntax_inc = nixf_inc

libnixfSyntax = library('nixfSyntax',
                        'RawSyntax.cpp',
                        'Trivia.cpp',
                        'Syntax.cpp',
                        'SyntaxData.cpp',
                        include_directories: libnixfSyntax_inc,
                        dependencies: libnixfSyntax_deps,
                        install: true
)

pkgconfig.generate(name: 'libnixfSyntax',
                   version: 'nightly',
                   description: 'libnixfSyntax',
                   libraries: libnixfSyntax
)

nixfSyntax = declare_dependency(include_directories: libnixfSyntax_inc,
                                link_with: libnixfSyntax,
                                dependencies: libnixfSyntax_deps
)
