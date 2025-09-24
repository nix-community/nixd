nixd = executable(
    'nixd',
    'nixd.cpp',
    install: true,
    dependencies: libnixd
)

lit = find_program('lit', required: false)

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

regression_controller_env = environment()

regression_controller_env.prepend('PATH', attrset_eval_dir)
regression_controller_env.prepend('PATH', meson.current_build_dir())
regression_controller_env.set('MESON_BUILD_ROOT', meson.current_build_dir())
regression_controller_env.set('NIXD_ATTRSET_EVAL', nixd_attrset_eval_prog.full_path())


if lit.found()
    test(
        'regression/nixd',
        lit,
        env: regression_controller_env,
        args: [
            '-vv',
            meson.current_source_dir() + '/nixd/test'
        ],
        timeout: 120,
        depends: [ nixd ] + nixd_attrset_eval_dep )
endif

regression_worker_env = environment()

regression_worker_env.append('PATH', meson.current_build_dir())
regression_worker_env.append('PATH', attrset_eval_dir)
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
        depends: nixd_attrset_eval_dep )
endif
