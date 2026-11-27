# RUN: sed 's|TEST_DIR|%S/Inputs|g' %s > %t && nixd --lit-test < %t | FileCheck %s

An import of a file that does not exist resolves nothing.

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
let
  libs = import ./attr-import-lib.nix { };
  raw = import ./attr-import-lib.nix;
  gone = import ./attr-import-missing.nix { };
in
{
  a = libs.helpers.triple 3;
  b = libs.double 2;
  c = raw.helpers;
  d = gone.x;
  e = libs.helpers.triple 4;
  inherit (libs) double;
}
```

<-- textDocument/definition(1) at 9:11, `d = gone.x;`

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
         "line": 9,
         "character": 11
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
