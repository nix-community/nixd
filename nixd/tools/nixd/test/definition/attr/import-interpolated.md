# RUN: sed 's|TEST_DIR|%S/Inputs|g' %s > %t && nixd --lit-test < %t | FileCheck %s

A path with an interpolation is not a literal: it is not followed.

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
let n = "lib"; s = import ./attr-import-${n}.nix { }; in s.double
```

<-- textDocument/definition(1) at 0:59, `let n = "lib"; s = import ./attr-import-${n}.nix { }; in s.double`

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
         "character": 59
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
