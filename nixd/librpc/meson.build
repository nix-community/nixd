libnixdrpc_inc = include_directories('include')

libnixdrpc_deps = [ libbc ]

libnixdrpc_lib = library('nixdrpc',
    'src/IO.cpp',
    'src/Transport.cpp',
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

