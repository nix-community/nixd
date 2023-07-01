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
# ^<-- request rename here (i.e. "xx")
  yy = 2;
in
{
  foo = xx;
  bar = xx;
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
         "text":"let\r\n  xx = 1;\r\n# ^<-- request rename here (i.e. \"xx\")\r\n  yy = 2;\r\nin\r\n{\r\n  foo = xx;\r\n  bar = xx;\r\n  baz = yy;\r\n}"
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
            "character": 2,
            "line": 1
        },
        "textDocument": {
            "uri": "file:///test.nix"
        }
    }
}
```

```
     CHECK:     "file:///test.nix": [
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
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 7
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 8,
CHECK-NEXT:              "line": 7
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
