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

Testing this nix file:

```nix
let
a = 1;
b = 2;
c = 3;
in
c
```

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///let.nix",
            "languageId": "nix",
            "version": 1,
            "text": "let\na = 1;\nb = 2;\nc = 3;\nin\nc"
        }
    }
}
```


```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didChange",
    "params": {
        "textDocument": {
            "uri": "file:///let.nix",
            "version": 2
        },
        "contentChanges": [
            {
                "range": {
                    "start": {
                        "line": 5,
                        "character": 0
                    },
                    "end": {
                        "line": 5,
                        "character": 1
                    }
                },
                "rangeLength": 1,
                "text": "a"
            }
        ]
    }
}
```


<-- textDocument/definition(1)

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file:///let.nix"
      },
      "position":{
         "line":5,
         "character":0
      }
   }
}
```


```
     CHECK: "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 0,
CHECK-NEXT:         "line": 1
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 0,
CHECK-NEXT:         "line": 1
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "uri": "file:///let.nix"
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
