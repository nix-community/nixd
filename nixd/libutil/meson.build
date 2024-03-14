libnixdutil_inc = include_directories('include')

libnixdutil_deps = [ boost ]

libnixdutil_lib = library('nixdutil',
    'src/AutoCloseFD.cpp',
    'src/AutoRemoveShm.cpp',
    'src/ForkPiped.cpp',
    install: true,
    include_directories: libnixdutil_inc,
    dependencies: libnixdutil_deps,
)

libnixdutil = declare_dependency(
    include_directories: libnixdutil_inc,
    link_with: libnixdutil_lib,
    dependencies: libnixdutil_deps,
)

