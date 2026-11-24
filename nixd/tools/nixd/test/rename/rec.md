# RUN: nixd --lit-test < %s | FileCheck %s

// Renaming a `rec` attribute renames the key the set exports, so it is refused

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
rec { double = x: x * 2; quad = x: double (double x); }
```

<-- textDocument/rename(2)


```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/rename",
   "params": {
      "textDocument": {
         "uri": "file:///basic.nix"
      },
      "position": {
         "line": 0,
         "character": 6
      },
      "newName": "twice"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT:   "code": -32001,
CHECK-NEXT:   "message": "cannot rename `rec` attribute: renaming it renames the attribute the set exports, which its users select by name; rename it by hand"
CHECK-NEXT: },
CHECK-NEXT: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
