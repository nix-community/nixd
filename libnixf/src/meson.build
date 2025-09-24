libnixf_inc = include_directories('../include')

libnixf_deps = [ boost, nlohmann_json, nixd_common ]

python3 = find_program('python3')

diagnostic_enum_h = custom_target(
    input: ['Basic/DiagnosticEnum.h.py', 'Basic/diagnostic.py', 'Basic/support.py'],
    output: 'DiagnosticEnum.h',
    command: [python3, '@INPUT0@', '@OUTPUT@'],
    install: true,
    install_dir: 'include/nixf/Basic/'
)


diagnostic_cpp = custom_target(
    input: ['Basic/Diagnostic.cpp.py', 'Basic/diagnostic.py', 'Basic/support.py'],
    output: 'Diagnostic.cpp',
    command: [python3, '@INPUT0@', '@OUTPUT@']
)

tokens_h = custom_target(
    input: ['Parse/Tokens.h.py', 'Parse/tokens.py'],
    output: 'Tokens.h',
    command: [python3, '@INPUT0@', '@OUTPUT@'],
    install: true,
    install_dir: 'include/nixf/Parse/'
)

tokens_kinds_inc = custom_target(
    input: ['Parse/TokenKinds.inc.py', 'Parse/tokens.py'],
    output: 'TokenKinds.inc',
    command: [python3, '@INPUT0@', '@OUTPUT@'],
    install: true,
    install_dir: 'include/nixf/Parse/'
)

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
    diagnostic_enum_h,
    diagnostic_cpp,
    tokens_h,
    tokens_kinds_inc,
    include_directories: libnixf_inc,
    dependencies: libnixf_deps,
    install: true,
    cpp_args: [ '-Werror=implicit-fallthrough' ]
)

pkgconfig.generate(
    name: 'nixf',
    version: 'nightly',
    description: 'libnixf',
    libraries: libnixf
)

nixf = declare_dependency(
    sources: [diagnostic_enum_h, tokens_h],
    include_directories: libnixf_inc,
    link_with: libnixf,
    dependencies: libnixf_deps
)


install_subdir('../include/nixf', install_dir: 'include')

meson.override_dependency('nixf', nixf)
