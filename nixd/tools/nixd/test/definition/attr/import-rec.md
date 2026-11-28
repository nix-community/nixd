# RUN: sed 's|TEST_DIR|%S/Inputs|g' %s > %t && nixd --lit-test < %t | FileCheck %s

An imported `rec` attribute.

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

<-- textDocument/definition(1) at 7:11, `b = libs.double 2;`

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
         "line": 7,
         "character": 11
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 8,
CHECK-NEXT: "line": 2
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 2,
CHECK-NEXT: "line": 2
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file://{{.*}}/attr-import-lib.nix"
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
