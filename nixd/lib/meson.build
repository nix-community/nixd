libnixd_include = include_directories('../include')

libnixd_deps = [ nixd_lsp_server, nixf, llvm, nixt ]

libnixd_lib = library(
    'nixd',
    'CommandLine/Options.cpp',
    'Controller/AST.cpp',
    'Controller/CodeAction.cpp',
    'Controller/Completion.cpp',
    'Controller/Configuration.cpp',
    'Controller/Convert.cpp',
    'Controller/Definition.cpp',
    'Controller/Diagnostics.cpp',
    'Controller/DocumentHighlight.cpp',
    'Controller/DocumentLink.cpp',
    'Controller/DocumentSymbol.cpp',
    'Controller/FindReferences.cpp',
    'Controller/Format.cpp',
    'Controller/Hover.cpp',
    'Controller/InlayHints.cpp',
    'Controller/LifeTime.cpp',
    'Controller/NixTU.cpp',
    'Controller/Rename.cpp',
    'Controller/SemanticTokens.cpp',
    'Controller/Support.cpp',
    'Controller/TextDocumentSync.cpp',
    'Eval/AttrSetClient.cpp',
    'Eval/AttrSetProvider.cpp',
    'Eval/Launch.cpp',
    'Protocol/AttrSet.cpp',
    'Protocol/Protocol.cpp',
    'Support/AutoCloseFD.cpp',
    'Support/AutoRemoveShm.cpp',
    'Support/ForkPiped.cpp',
    'Support/StreamProc.cpp',
    dependencies: libnixd_deps,
    include_directories: libnixd_include,
    install: true
)

libnixd = declare_dependency(
    include_directories: libnixd_include,
    link_with: libnixd_lib,
    dependencies: libnixd_deps
)
