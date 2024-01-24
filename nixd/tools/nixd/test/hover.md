# RUN: nixd --lit-test < %s | FileCheck %s

<-- initialize(0)

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"initialize",
   "params":{
      "processId":123,
      "rootPath":"",
      "capabilities":{
      },
      "trace":"off"
   }
}
```


<-- textDocument/didOpen

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"{ a = 1; }"
      }
   }
}
```

<-- textDocument/hover(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/hover",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
         "line":0,
         "character":2
      }
   }
}
```

```
     CHECK:  "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "contents": {
CHECK-NEXT:      "kind": "markdown",
CHECK-NEXT:      "value": "`Identifer`"
CHECK-NEXT:    },
CHECK-NEXT:    "range": {
CHECK-NEXT:      "end": {
CHECK-NEXT:        "character": 3,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      },
CHECK-NEXT:      "start": {
CHECK-NEXT:        "character": 2,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  }
```

<-- textDocument/hover(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/hover",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
         "line":0,
         "character":4
      }
   }
}
```

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "contents": {
CHECK-NEXT:      "kind": "markdown",
CHECK-NEXT:      "value": "`Binding`"
CHECK-NEXT:    },
CHECK-NEXT:    "range": {
CHECK-NEXT:      "end": {
CHECK-NEXT:        "character": 8,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      },
CHECK-NEXT:      "start": {
CHECK-NEXT:        "character": 2,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
