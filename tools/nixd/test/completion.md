# RUN: nixd -wait-worker 1000000 --lit-test < %s | FileCheck %s

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
  };
in
with pkgs;

a
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
         "text":"let\n  pkgs = {\n    a = 1;\n  };\nin\nwith pkgs;\n\na\n\n"
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
            "line": 3,
            "character": 4
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK:    "isIncomplete": false,
CHECK-NEXT:    "items": [
CHECK-NEXT:      {
CHECK-NEXT:        "kind": 21,
CHECK-NEXT:        "label": "builtins",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
