libnixdSupportDeps = [ nix_all
                     , llvm
                     , nixdParser
                     , nixd_lsp_server
                     ]

libnixdSupport = library('nixdSupport'
, 'CompletionHelper.cpp'
, 'Diagnostic.cpp'
, 'JSONSerialization.cpp'
, include_directories: nixd_inc
, dependencies: libnixdSupportDeps
, install: true
)


nixdSupport = declare_dependency( include_directories: nixd_inc
                                , dependencies: libnixdSupportDeps
                                , link_with: libnixdSupport
                                )
