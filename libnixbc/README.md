# libnixbc

Defines general bytecode for nix AST.
This is used for nix-interop, with C++ evaluator, thus the node definition is targeted for C++ types.


## The format


All AST nodes are encoded in a compact binary format.
The entire AST is serialized into into a form similar to [S-expression](https://en.wikipedia.org/wiki/S-expression).


### Node Format

| Field Name | Byte Size
|:-:|:-:|
| Pointer | Pointer Size
| Kind | 4
| Payloads | N/A
