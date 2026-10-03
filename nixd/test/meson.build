test(
    'unit/fork-piped',
    executable(
        'test-fork-piped',
        'ForkPiped.cpp',
        '../lib/Support/ForkPiped.cpp',
        include_directories: libnixd_include,
        dependencies: [ gtest, nix_util, dependency('threads') ],
    ),
    timeout: 30,
)
