test(
    'unit/fork-piped',
    executable(
        'test-fork-piped',
        'ForkPiped.cpp',
        '../lib/Support/ForkPiped.cpp',
        include_directories: libnixd_include,
        dependencies: [ gtest, dependency('threads') ],
    ),
    timeout: 30,
)
