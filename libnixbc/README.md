# libnixbc

Defines general bytecode for nix AST.
This is used for nix-interop, with C++ evaluator, thus the node definition is targeted for C++ types.


## The format


All AST nodes are encoded in a compact binary format.
The entire AST is serialized into into a form similar to [S-expression](https://en.wikipedia.org/wiki/S-expression).


### Node Format

The first field is "Kind", with 4 bytes, and the second field is "Pointer", for furthur references.


| Field Name | Byte Size
|:-:|:-:|
| Kind | 4
| Pointer | Pointer Size
| Payloads | N/A
