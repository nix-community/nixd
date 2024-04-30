## `libnixf`, The nix frontend

This is a nix language frontend (lexer + parser, semantic analysis) focusing on editing experience.

## Modules

Currently the package has these following components.

* Basic (Diagnostic Types, Node Types, ...)
* Parse (The parsing algorithm implementation)
* Sema (Semantic Analysis, e.g. duplicated keys)
* Bytecode (experimental)

