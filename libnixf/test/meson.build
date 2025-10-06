test('unit/libnixf/Basic',
    executable('unit-libnixf-basic',
        'Basic/Diagnostic.cpp',
        'Basic/Nodes.cpp',
        dependencies: [ nixf, gtest_main ],
    )
)


test('unit/libnixf/Parse',
    executable('unit-libnixf-parse',
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

common_sp = subproject('common', required : false)
if common_sp.found()
    nixd_attrset_eval_prog = common_sp.get_variable('nixd_attrset_eval')
    nixd_attrset_eval_dep = [nixd_attrset_eval_prog]
else
    nixd_attrset_eval_prog = find_program('nixd-attrset-eval', required : true)
    nixd_attrset_eval_dep = []
endif

fs = import('fs')
attrset_eval_dir = fs.parent(nixd_attrset_eval_prog.full_path())

libnixf_test_env = environment()

libnixf_test_env.prepend('PATH', attrset_eval_dir)
libnixf_test_env.prepend('PATH', meson.current_build_dir())
libnixf_test_env.set('MESON_BUILD_ROOT', meson.current_build_dir())
libnixf_test_env.set('NIXD_ATTRSET_EVAL', nixd_attrset_eval_prog.full_path())

test('unit/libnixf/Sema',
    executable('unit-libnixf-sema',
        'Sema/SemaActions.cpp',
        'Sema/VariableLookup.cpp',
        'Sema/ParentMap.cpp',
        dependencies: [ nixf, gtest_main ],
        include_directories: [ '../src/Sema' ] # Private headers
    ),
    timeout: 120,
    env: libnixf_test_env,
    depends: nixd_attrset_eval_dep,
)
