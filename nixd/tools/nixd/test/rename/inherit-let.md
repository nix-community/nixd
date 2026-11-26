# RUN: nixd --lit-test < %s | FileCheck %s

// A `let` variable from `inherit greet;` reads the outer `greet`, so renaming it keeps that: `welcome = greet;`

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
let greet = 1; in let inherit greet; in greet
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
         "character": 41
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
CHECK-NEXT:             "character": 45,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 40,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       {
CHECK-NEXT:         "newText": "welcome = greet;",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 36,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 22,
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
