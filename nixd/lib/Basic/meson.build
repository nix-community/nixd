libnixdBasicDeps = [ nix_all, llvm ]
libnixdBasic = library('nixdBasic'
, [ 'Diagnostic.cpp'
  ]
, include_directories: nixd_inc
, dependencies: libnixdBasicDeps
, install: true
)

nixdBasic = declare_dependency( include_directories: nixd_inc
                            , dependencies: libnixdBasicDeps
                            , link_with: libnixdBasic
                            )
