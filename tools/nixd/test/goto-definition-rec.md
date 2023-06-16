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
rec {
    x = 1;
#   ^
    y = x;
    z = y;
}
```

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///rec.nix",
            "languageId": "nix",
            "version": 1,
            "text": "rec {\n    x = 1;\n    y = x;\n    z = y;\n}\n"
        }
    }
}
```

<-- textDocument/definition

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file:///rec.nix"
      },
      "position":{
         "line":3,
         "character":8
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
CHECK-NEXT:         "character": 4,
CHECK-NEXT:         "line": 2
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 4,
CHECK-NEXT:         "line": 2
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "uri": "file:///rec.nix"
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
