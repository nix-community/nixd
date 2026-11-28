# RUN: nixd --lit-test < %s | FileCheck %s

A flake's injected `self`, with no uses, is checked like any formal: `y` is in the same scope.

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

```nix file:///flake.nix
{ outputs = { self, y ? 2 }: y; }
```

<-- textDocument/rename(1) at 0:14, `{ outputs = { self, y ? 2 }: y; }`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/rename",
   "params": {
      "textDocument": {
         "uri": "file:///flake.nix"
      },
      "position": {
         "line": 0,
         "character": 14
      },
      "newName": "y"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "cannot rename: the `y` bound at line 1, column 21 is in the same scope"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
