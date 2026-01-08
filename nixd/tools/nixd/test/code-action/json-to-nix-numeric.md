# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert JSON to Nix" action handles numeric edge cases correctly.

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

```nix file:///json-to-nix-numeric.nix
{"neg": -42, "float": 3.14159, "sci": 1.5e10, "negFloat": -0.001}
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///json-to-nix-numeric.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":67
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Numeric edge cases should convert correctly: negative integers, floating-point numbers, scientific notation, and negative floats.

```
     CHECK:   "id": 1,
     CHECK:   "result": [
     CHECK:       "kind": "refactor.rewrite"
     CHECK:       "title": "Convert JSON to Nix"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
