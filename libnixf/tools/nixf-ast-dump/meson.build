nixf_ast_dump = executable('nixf-ast-dump', ['nixf-ast-dump.cpp'],
                           dependencies: [ nixfParse ],
                           install: true)

regression_env = environment()

regression_env.append('PATH', meson.current_build_dir())
regression_env.set('MESON_BUILD_ROOT', meson.current_build_dir())

if lit.found()
    test('regression/nixf-ast-dump', lit, env: regression_env, args: [ '-vv', meson.current_source_dir() + '/test' ], depends: [ nixf_ast_dump ] )
endif
