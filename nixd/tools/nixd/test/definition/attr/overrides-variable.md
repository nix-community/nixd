# RUN: nixd --lit-test < %s | FileCheck %s

A variable bound by a `rec` set with `__overrides` is not an attribute to resolve: nothing after it is.

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
rec {
  __overrides = { };
  a = { x = 1; };
  b = a.x;
}
```

<-- textDocument/definition(1) at 3:8, `b = a.x;`

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
         "line": 3,
         "character": 8
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
