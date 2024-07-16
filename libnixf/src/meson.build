libnixf_deps = [ boost, nlohmann_json ]

libnixf_inc = include_directories('../include')

libnixf = library(
    'nixf',
    'Basic/Nodes.cpp',
    'Basic/JSONDiagnostic.cpp',
    'Basic/Diagnostic.cpp',
    'Parse/Lexer.cpp',
    'Parse/ParseAttrs.cpp',
    'Parse/ParseExpr.cpp',
    'Parse/ParseLambda.cpp',
    'Parse/ParseOp.cpp',
    'Parse/ParseSimple.cpp',
    'Parse/ParseStrings.cpp',
    'Parse/ParseSupport.cpp',
    'Sema/ParentMap.cpp',
    'Sema/SemaActions.cpp',
    'Sema/VariableLookup.cpp',
    include_directories: libnixf_inc,
    dependencies: libnixf_deps,
    install: true,
    cpp_args: [ '-Werror=implicit-fallthrough' ]
)

pkgconfig.generate(
    name: 'nixf',
    version: '2.2.3',
    description: 'libnixf',
    libraries: libnixf
)

nixf = declare_dependency(
    include_directories: libnixf_inc,
    link_with: libnixf,
    dependencies: libnixf_deps
)


install_subdir('../include/nixf', install_dir: 'include')
