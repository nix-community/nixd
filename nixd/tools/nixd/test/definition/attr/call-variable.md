# RUN: nixd --lit-test < %s | FileCheck %s

The variable `f` is not the builtin `import`: its call is not followed.

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
let f = x: x; s = f { a = 1; }; in s.a
```

<-- textDocument/definition(1) at 0:37, `let f = x: x; s = f { a = 1; }; in s.a`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/definition",
   "params": {
      "textDocument": {
         "uri": "file:///main.nix"
      },
      "position": {
         "line": 0,
         "character": 37
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": null
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
