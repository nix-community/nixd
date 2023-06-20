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

File #1:

```nix
let
  pkgs = {
    a = 1;
    foo = 1;
    bar = 1;
    fo = 2;
  };
in
with pkgs;

{
  x = 1;
# | <- request completion here
}
```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///completion.nix",
         "languageId":"nix",
         "version":1,
         "text":"let\r\n  pkgs = {\r\n    a = 1;\r\n    foo = 1;\r\n    bar = 1;\r\n    fo = 2;\r\n  };\r\nin\r\nwith pkgs;\r\n\r\n{\r\n  x = 1;\r\n# | <- request completion here\r\n}"
      }
   }
}
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file:///completion.nix"
        },
        "position": {
            "line": 12,
            "character": 3
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK:        "label": "pkgs",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "kind": 6,
CHECK-NEXT:        "label": "a",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "kind": 6,
CHECK-NEXT:        "label": "foo",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "kind": 6,
CHECK-NEXT:        "label": "bar",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "kind": 6,
CHECK-NEXT:        "label": "fo",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
