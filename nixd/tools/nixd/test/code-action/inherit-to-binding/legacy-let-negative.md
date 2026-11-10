# RUN: nixd --lit-test < %s | FileCheck %s

`let { }` is recursive too: `x = x;` there would read itself: no action.

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

```nix file:///legacy-let.nix
let x = 1; y = 2; in let { inherit x y; body = x; }
```

<-- textDocument/codeAction(2) at 0:35

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///legacy-let.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 35
         },
         "end": {
            "line": 0,
            "character": 35
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
