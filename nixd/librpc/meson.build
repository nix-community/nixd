libnixdrpc_inc = include_directories('include')

libnixdrpc_deps = [ libbc, nixd_lsp_server ]

libnixdrpc_lib = library('nixdrpc',
    'src/Protocol.cpp',
    install: true,
    include_directories: libnixdrpc_inc,
    dependencies: libnixdrpc_deps,
)

libnixdrpc = declare_dependency(
    include_directories: libnixdrpc_inc,
    link_with: libnixdrpc_lib,
    dependencies: libnixdrpc_deps,
)

test('unit/nixd/librpc',
    executable(
        'unit-nixd-librpc',
        'test/Transport.cpp',
        dependencies: [ gtest_main, libnixdrpc ]
    )
)
