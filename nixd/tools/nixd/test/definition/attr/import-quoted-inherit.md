# RUN: sed 's|TEST_DIR|%S/Inputs|g' %s > %t && nixd --lit-test < %t | FileCheck %s

An imported file with a quoted `inherit` name is parsed without crashing.

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

```nix file://TEST_DIR/quoted-inherit-main.nix
let s = import ./quoted-inherit.nix; in s.x
```

<-- textDocument/definition(1) at 0:42, `let s = import ./quoted-inherit.nix; in s.x`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/definition",
   "params": {
      "textDocument": {
         "uri": "file://TEST_DIR/quoted-inherit-main.nix"
      },
      "position": {
         "line": 0,
         "character": 42
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
CHECK-NEXT: "character": 13,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 10,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file://{{.*}}/quoted-inherit.nix"
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
