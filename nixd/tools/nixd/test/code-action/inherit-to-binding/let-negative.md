# RUN: nixd --lit-test < %s | FileCheck %s

`inherit x;` in a `let` reads `x` from outside it; `x = x;` would read itself: no action.

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

```nix file:///let.nix
let x = 1; in let inherit x; in x
```

<-- textDocument/codeAction(2) at 0:26

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///let.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 26
         },
         "end": {
            "line": 0,
            "character": 26
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
