# RUN: sed 's|TEST_DIR|%S/Inputs|g' %s > %t && nixd --lit-test < %t | FileCheck %s

The file has one top lambda: a second argument applies the set it returns, so nothing resolves.

<-- initialize(0)

```json
{
   "jsonrpc": "2.0",
   "id": 0,
   "method": "initialize",
   "params": {
      "processId": 123,
      "rootPath": "",
      "capabilities": {},
      "trace": "off"
   }
}
```



<-- textDocument/didOpen

```nix file://TEST_DIR/attr-import-main.nix
let s = import ./attr-import-lib.nix { } { }; in s.double
```

<-- textDocument/definition(1) at 0:51, `let s = import ./attr-import-lib.nix { } { }; in s.double`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/definition",
   "params": {
      "textDocument": {
         "uri": "file://TEST_DIR/attr-import-main.nix"
      },
      "position": {
         "line": 0,
         "character": 51
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": null
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
