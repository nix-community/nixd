# RUN: nixd --lit-test < %s | FileCheck %s

So is one from `inherit (e) "__curPos" b;`: the name is decided by its value, not its spelling.

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

```nix file:///main.nix
let e = { __curPos = 5; b = 2; }; inherit (e) "__curPos" b; in __curPos
```

<-- textDocument/rename(1) at 0:63, `let e = { __curPos = 5; b = 2; }; inherit (e) "__curPos" b; in __curPos`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/rename",
   "params": {
      "textDocument": {
         "uri": "file:///main.nix"
      },
      "position": {
         "line": 0,
         "character": 63
      },
      "newName": "y"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "cannot rename `__curPos`: written as an expression it is the current position, not this binding"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
