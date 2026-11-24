# RUN: nixd --lit-test < %s | FileCheck %s

`inherit __curPos;` reads the variable `__curPos`; `__curPos = __curPos;` would be the position it is written at: no action.

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

```nix file:///curpos.nix
let __curPos = 1; in { inherit __curPos; }
```

<-- textDocument/codeAction(2) at 0:31

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///curpos.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 31
         },
         "end": {
            "line": 0,
            "character": 31
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
