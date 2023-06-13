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

Testing this nix file, a dummy derivation.

```nix
let
  foo = {
    type = "derivation";
    meta.position = "/foo.nix:4";
  };
in
foo
#^ expected location: foo.nix:4
```

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///derivation.nix",
            "languageId": "nix",
            "version": 1,
            "text": "let\r\n  foo = {\r\n    type = \"derivation\";\r\n    meta.position = \"\/foo.nix:4\";\r\n  };\r\nin\r\nfoo"
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
         "uri":"file:///derivation.nix"
      },
      "position":{
         "line":6,
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
CHECK-NEXT:         "line": 4
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 0,
CHECK-NEXT:         "line": 4
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "uri": "file:///foo.nix"
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
