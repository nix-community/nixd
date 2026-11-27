# RUN: sed 's|TEST_DIR|%S/Inputs|g' %s > %t && nixd --lit-test < %t | FileCheck %s

An imported file that is a lambda without a body, applied to an argument, resolves nothing.

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

```nix file://TEST_DIR/attr-import-lib.nix
x:
```


<-- textDocument/didOpen

```nix file://TEST_DIR/attr-import-main.nix
let s = import ./attr-import-lib.nix { }; in s.double
```

<-- textDocument/definition(1) at 0:47, `let s = import ./attr-import-lib.nix { }; in s.double`

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
         "character": 47
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
