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
    "method": "textDocument/prepareRename",
    "params": {
        "position": {
            "character": 2,
            "line": 0
        },
        "textDocument": {
            "uri": "file:///test.nix"
        }
    }
}
```

```
     CHECK:    "message": "no rename edits available"
CHECK-NEXT:  },
CHECK-NEXT:  "id": 12,
CHECK-NEXT:  "jsonrpc": "2.0"
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
