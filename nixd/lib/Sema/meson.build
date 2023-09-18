libnixdSemaDeps = [ nixdSyntax ]

libnixdSema = library('nixdSema'
, 'Lowering.cpp'
, include_directories: nixd_inc
, dependencies: libnixdSemaDeps
, install: true
)

nixdSema = declare_dependency( include_directories: nixd_inc
                             , link_with: libnixdSema
                             , dependencies: libnixdSemaDeps
                             )
