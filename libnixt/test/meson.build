test(
    'unit/libnixt',
    executable(
        'test-libnixt',
        'InitEval.cpp',
        'Value.cpp',
        dependencies: [ gtest_main, nixt ]
    ),
    env: [ 'ASAN_OPTIONS=detect_leaks=0']
)
