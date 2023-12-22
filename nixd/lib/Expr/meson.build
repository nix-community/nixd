libnixdExprDeps = [ nix_all, nixt ]

libnixdExpr = library('nixdExpr'
, 'Expr.cpp'
, 'CallbackExpr.cpp'
, 'Nodes.cpp'
, include_directories: nixd_inc
, dependencies: libnixdExprDeps
, install: true
)

nixdExpr = declare_dependency( include_directories: nixd_inc
                             , dependencies: libnixdExprDeps
                             , link_with: libnixdExpr
                             )
