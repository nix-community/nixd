# RUN: nixd --lit-test < %s | FileCheck %s

// `inherit greet;` names the attribute, so it keeps it: `greet = welcome;`

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
let greet = 1; in { inherit greet; }
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
         "character": 4
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
CHECK-NEXT:         "newText": "greet = welcome;",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 34,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 20,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       {
CHECK-NEXT:         "newText": "welcome",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 9,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 4,
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
