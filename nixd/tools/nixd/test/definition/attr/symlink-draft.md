# RUN: mkdir -p %t.dir && printf '{ chosen = 1; }\n' > %t.dir/target.nix && ln -sf %t.dir/target.nix %t.dir/alias.nix && sed 's|TEST_DIR|%t.dir|g' %s > %t && nixd --lit-test < %t | FileCheck %s

An import reaching an open document through a symlink uses the open document, not the file on disk.

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

```nix file://TEST_DIR/alias.nix
{ unsaved = 0; chosen = 2; }
```

<-- textDocument/didOpen

```nix file://TEST_DIR/main.nix
let s = import ./alias.nix; in s.chosen
```

<-- textDocument/definition(1) at 0:33, `let s = import ./alias.nix; in s.chosen`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/definition",
   "params": {
      "textDocument": {
         "uri": "file://TEST_DIR/main.nix"
      },
      "position": {
         "line": 0,
         "character": 33
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
CHECK-NEXT: "character": 21,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 15,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file://{{.*}}/alias.nix"
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
