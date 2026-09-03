# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ lib.hello = 1; }" \
# RUN: < %s | FileCheck %s

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

```nix file:///completion.nix
lib.
```

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "textDocument/completion",
  "params": {
    "textDocument": {
      "uri": "file:///completion.nix"
    },
    "position": {
      "line": 0,
      "character": 4
    },
    "context": {
      "triggerKind": 2,
      "triggerCharacter": "."
    }
  }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "isIncomplete": true,
CHECK-NEXT:   "items": []
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
