# RUN: nixd --lit-test < %s | FileCheck %s

Regression for #907: quoted inherit names retain variable lookup and source ranges.

```json
{
  "jsonrpc": "2.0",
  "id": 0,
  "method": "initialize",
  "params": {
    "processId": 123,
    "rootPath": "",
    "capabilities": {}
  }
}
```

```nix file:///quoted-inherit.nix
let path = 1; in { inherit "path"; }
```

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "textDocument/references",
  "params": {
    "textDocument": {
      "uri": "file:///quoted-inherit.nix"
    },
    "position": {
      "line": 0,
      "character": 28
    }
  }
}
```

```
CHECK:   "id": 3,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 33,
CHECK-NEXT:           "line": 0
CHECK-NEXT:         },
CHECK-NEXT:         "start": {
CHECK-NEXT:           "character": 27,
CHECK-NEXT:           "line": 0
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       "uri": "file:///quoted-inherit.nix"
CHECK-NEXT:     }
CHECK-NEXT:   ]
CHECK-NEXT: }
```

```json
{
  "jsonrpc": "2.0",
  "method": "exit"
}
```
