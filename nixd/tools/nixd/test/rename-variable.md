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


```nix
let
  xx = 1;
  yy = 2;
in
{
  foo = xx;
  bar = xx;
  #     ^<-- request rename here (i.e. "xx")
  baz = yy;
}
```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///test.nix",
         "languageId":"nix",
         "version":1,
         "text":"let\r\n  xx = 1;\r\n  yy = 2;\r\nin\r\n{\r\n  foo = xx;\r\n  bar = xx;\r\n  #     ^<-- Rename request here (i.e. \"xx\")\r\n  baz = yy;\r\n}"
      }
   }
}
```

```json
{
    "id": 12,
    "jsonrpc": "2.0",
    "method": "textDocument/rename",
    "params": {
        "newName": "rr",
        "position": {
            "character": 9,
            "line": 6
        },
        "textDocument": {
            "uri": "file:///test.nix"
        }
    }
}
```

```
     CHECK:      "changes": {
CHECK-NEXT:      "file:///test.nix": [
CHECK-NEXT:        {
CHECK-NEXT:          "newText": "rr",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 8,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        {
CHECK-NEXT:          "newText": "rr",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 8,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        {
CHECK-NEXT:          "newText": "rr",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 4,
CHECK-NEXT:              "line": 1
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 2,
CHECK-NEXT:              "line": 1
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ]
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
