# RUN: sed 's|TEST_DIR|%S/../../definition/attr/Inputs|g' %s > %t && nixd --lit-test < %t | FileCheck %s

The key of an imported attribute is in the imported file.

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

```nix file://TEST_DIR/decl-main.nix
let libs = import ./attr-import-lib.nix { }; in libs.helpers.triple 3
```

<-- textDocument/references(1) at 0:61, `let libs = import ./attr-import-lib.nix { }; in libs.helpers.triple 3`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/references",
   "params": {
      "textDocument": {
         "uri": "file://TEST_DIR/decl-main.nix"
      },
      "position": {
         "line": 0,
         "character": 61
      },
      "context": {
         "includeDeclaration": true
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
CHECK-NEXT: {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 10,
CHECK-NEXT: "line": 4
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 4,
CHECK-NEXT: "line": 4
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file://{{.*}}/attr-import-lib.nix"
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 67,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 61,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file://{{.*}}/decl-main.nix"
CHECK-NEXT: }
CHECK-NEXT: ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
