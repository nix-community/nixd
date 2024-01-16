libnixf_deps = [ ]

libnixf_inc = include_directories('include')

libnixf = library(
    'nixf',
    'lib/Basic/Diagnostic.cpp',
    'lib/Parse/Lexer.cpp',
    'lib/Parse/Parser.cpp',
    include_directories: libnixf_inc,
    dependencies: libnixf_deps,
    install: true,
    cpp_args: [ '-Werror=implicit-fallthrough' ]
)

pkgconfig.generate(
    name: 'libnixf',
    version: 'nightly',
    description: 'libnixf',
    libraries: libnixf
)

nixf = declare_dependency(
    include_directories: libnixf_inc,
    link_with: libnixf,
    dependencies: libnixf_deps
)

test('unit/libnixf/Parse',
    executable('unit-libnixf-parse',
        'lib/Parse/test/Lexer.cpp',
        'lib/Parse/test/Parser.cpp',
        dependencies: [ nixf, gtest_main ],
        include_directories: [ 'lib/Parse' ] # Private headers
    )
)


install_subdir('include/nixf', install_dir: 'include')
