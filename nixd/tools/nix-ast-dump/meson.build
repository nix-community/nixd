nix_ast_dump = executable('nix-ast-dump', ['nix-ast-dump.cpp']
, dependencies: [ boost
                , nixdDeps
                , nix_all
                ]
)

regression_env = environment()

regression_env.append('PATH', meson.current_build_dir())
regression_env.set('MESON_BUILD_ROOT', meson.current_build_dir())

test('regression/nix-ast-dump', lit, env: regression_env, args: [ '-vv', meson.current_source_dir() + '/test' ], depends: [ nix_ast_dump ] )
