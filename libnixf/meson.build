libnixf_deps = [ boost, libnixbc, nlohmann_json ]

libnixf_inc = include_directories('include')

libnixf = library(
    'nixf',
    'src/Basic/Nodes.cpp',
    'src/Basic/JSONDiagnostic.cpp',
    'src/Basic/Diagnostic.cpp',
    'src/Bytecode/Write.cpp',
    'src/Parse/Lexer.cpp',
    'src/Parse/ParseAttrs.cpp',
    'src/Parse/ParseExpr.cpp',
    'src/Parse/ParseLambda.cpp',
    'src/Parse/ParseOp.cpp',
    'src/Parse/ParseSimple.cpp',
    'src/Parse/ParseStrings.cpp',
    'src/Parse/ParseSupport.cpp',
    'src/Sema/ParentMap.cpp',
    'src/Sema/SemaActions.cpp',
    'src/Sema/VariableLookup.cpp',
    include_directories: libnixf_inc,
    dependencies: libnixf_deps,
    install: true,
    cpp_args: [ '-Werror=implicit-fallthrough' ]
)

pkgconfig.generate(
    name: 'libnixf',
    version: '2.0.2',
    description: 'libnixf',
    libraries: libnixf
)

nixf = declare_dependency(
    include_directories: libnixf_inc,
    link_with: libnixf,
    dependencies: libnixf_deps
)

test('unit/libnixf/Basic',
    executable('unit-libnixf-basic',
        'test/Basic/Diagnostic.cpp',
        'test/Basic/Nodes.cpp',
        dependencies: [ nixf, gtest_main ],
    )
)

test('unit/libnixf/Bytecode',
    executable('unit-libnixf-bytecode',
        'test/Bytecode/Write.cpp',
        dependencies: [ nixf, gtest_main ],
    )
)

test('unit/libnixf/Parse',
    executable('unit-libnixf-parse',
        'test/Parse/Lexer.cpp',
        'test/Parse/ParseAttrs.cpp',
        'test/Parse/ParseExpr.cpp',
        'test/Parse/ParseLambda.cpp',
        'test/Parse/ParseOp.cpp',
        'test/Parse/ParseSimple.cpp',
        dependencies: [ nixf, gtest_main ],
        include_directories: [ 'src/Parse' ] # Private headers
    )
)

test('unit/libnixf/Sema',
    executable('unit-libnixf-sema',
        'test/Sema/SemaActions.cpp',
        'test/Sema/VariableLookup.cpp',
        'test/Sema/ParentMap.cpp',
        dependencies: [ nixf, gtest_main ],
        include_directories: [ 'src/Sema' ] # Private headers
    )
)

subdir('tools')

nixf_tidy = executable('nixf-tidy',
    'tools/nixf-tidy.cpp',
    dependencies: [ nixf ],
    install: true
)

install_subdir('include/nixf', install_dir: 'include')
