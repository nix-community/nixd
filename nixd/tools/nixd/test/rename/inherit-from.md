# RUN: nixd --lit-test < %s | FileCheck %s

// `inherit (x) greet;` selects `x.greet`, so renaming the variable keeps the selection: `welcome = x.greet;`

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
let x = { greet = 1; }; inherit (x) greet; in greet
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
         "character": 47
      },
      "newName": "welcome"
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "changes": {
CHECK-NEXT:     "file:///basic.nix": [
CHECK-NEXT:       {
CHECK-NEXT:         "newText": "welcome",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 51,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 46,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       {
CHECK-NEXT:         "newText": "welcome = x.greet;",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 42,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 24,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         }
CHECK-NEXT:       }
CHECK-NEXT:     ]
CHECK-NEXT:   }
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
