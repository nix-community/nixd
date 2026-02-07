# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action works correctly within let-in expressions.

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

```nix file:///pack-attrs-let.nix
let x = { foo.bar = 1; }; in x
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-let.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":10
         },
         "end":{
            "line":0,
            "character":17
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Pack should work inside let binding values.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "foo = { bar = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
