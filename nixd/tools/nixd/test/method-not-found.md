# RUN: nixd --lit-test < %s | FileCheck %s

A request nixd has no handler for is answered with MethodNotFound, and the session goes on: the request after it
is still answered.

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

```nix file:///basic.nix
{ a = 1; }
```

<-- textDocument/implementation(1)

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/implementation",
   "params": {
      "textDocument": {
         "uri": "file:///basic.nix"
      },
      "position": {
         "line": 0,
         "character": 2
      }
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT:   "code": -32601,
CHECK-NEXT:   "message": "method not found: textDocument/implementation"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

<-- textDocument/documentSymbol(2)

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/documentSymbol",
   "params": {
      "textDocument": {
         "uri": "file:///basic.nix"
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
CHECK:      "name": "a",
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
