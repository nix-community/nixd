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
         "text":"{ ${builtins.foo} = bar; }"
      }
   }
}
```

<-- textDocument/documentSymbol(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/documentSymbol",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      }
   }
}
```

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "deprecated": true,
CHECK-NEXT:          "detail": "identifier",
CHECK-NEXT:          "kind": 13,
CHECK-NEXT:          "name": "bar",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 23,
CHECK-NEXT:              "line": 0
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 20,
CHECK-NEXT:              "line": 0
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 23,
CHECK-NEXT:              "line": 0
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 20,
CHECK-NEXT:              "line": 0
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "${dynamic attribute}",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 23,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 17,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
