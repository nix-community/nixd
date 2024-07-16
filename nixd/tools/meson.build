nixd = executable(
    'nixd',
    'nixd.cpp',
    install: true,
    dependencies: libnixd
)

lit = find_program('lit', required: false)

nixd_attrset_eval = executable(
    'nixd-attrset-eval',
    'nixd-attrset-eval.cpp',
    install: true,
    dependencies: libnixd,
    install_dir: get_option('libexecdir'),
)

regression_controller_env = environment()

regression_controller_env.prepend('PATH', meson.current_build_dir())
regression_controller_env.set('MESON_BUILD_ROOT', meson.current_build_dir())
regression_controller_env.set('NIXD_ATTRSET_EVAL', nixd_attrset_eval.path())


if lit.found()
    test(
        'regression/nixd',
        lit,
        env: regression_controller_env,
        args: [
            '-vv',
            meson.current_source_dir() + '/nixd/test'
        ],
        timeout: 60,
        depends: [ nixd, nixd_attrset_eval ] )
endif

regression_worker_env = environment()

regression_worker_env.append('PATH', meson.current_build_dir())
regression_worker_env.set('MESON_BUILD_ROOT', meson.current_build_dir())
regression_worker_env.set('ASAN_OPTIONS', 'detect_leaks=0')

if lit.found()
    test(
        'regression/nixd-attrset-eval',
        lit,
        env: regression_worker_env,
        args: [
            '-vv',
            meson.current_source_dir() + '/nixd-attrset-eval/test'
        ],
        depends: [ nixd_attrset_eval ] )
endif
