libnixdNixDeps = [ nix_all, llvm ]
libnixdNix = library('nixdNix'
, [ 'EvalState.cpp'
  , 'Option.cpp'
  , 'Value.cpp'
  ]
, include_directories: nixd_inc
, dependencies: libnixdNixDeps
, install: true
)

nixdNix = declare_dependency( include_directories: nixd_inc
                            , dependencies: libnixdNixDeps
                            , link_with: libnixdNix
                            )
