libnixfParse_deps = [ nixfBasic ]

libnixfParse_inc = nixf_inc

libnixfParse = library('nixfParse',
                       'Lexer.cpp',
                       'Parser.cpp',
                       include_directories: libnixfParse_inc,
                       dependencies: libnixfParse_deps,
                       install: true,
                       cpp_args: [ '-Werror=implicit-fallthrough' ]
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



test('unit/libnixf/Parse',
    executable('libnixf-Parse',
        'test/Lexer.cpp',
        'test/Parser.cpp',
        dependencies: [ nixfParse, gtest_main ],
        include_directories: include_directories('.')
    )
)
