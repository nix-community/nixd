test('unit/libnixf/Basic',
    executable('unit-libnixf-basic',
        'Basic/Diagnostic.cpp',
        'Basic/Nodes.cpp',
        dependencies: [ nixf, gtest_main ],
    )
)


test('unit/libnixf/Parse',
    executable('unit-libnixf-parse',
        'Parse/Comment.cpp',
        'Parse/Lexer.cpp',
        'Parse/ParseAttrs.cpp',
        'Parse/ParseExpr.cpp',
        'Parse/ParseLambda.cpp',
        'Parse/ParseOp.cpp',
        'Parse/ParseSimple.cpp',
        dependencies: [ nixf, gtest_main ],
        include_directories: [ '../src/Parse' ] # Private headers
    )
)

test('unit/libnixf/Sema',
    executable('unit-libnixf-sema',
        'Sema/SemaActions.cpp',
        'Sema/VariableLookup.cpp',
        dependencies: [ nixf, gtest_main ],
        include_directories: [ '../src/Sema' ] # Private headers
    )
)
