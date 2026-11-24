# RUN: nixd --lit-test < %s | FileCheck %s

With several names, a source that is not a variable would be copied and evaluated again, a value of its own (`r.a == r.b` turns false): no action.

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

```nix file:///unshared.nix
let r = { inherit (let v = { f = x: x; }; in { a = v; b = v; }) a b; }; in r.a == r.b
```

<-- textDocument/codeAction(2) at 0:64

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///unshared.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 64
         },
         "end": {
            "line": 0,
            "character": 64
         }
      },
      "context": {
         "diagnostics": [],
         "triggerKind": 2
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
 CHECK-NOT: "title": "Convert to explicit binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
