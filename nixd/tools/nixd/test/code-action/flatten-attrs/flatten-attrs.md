# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Flatten nested attribute set" action is offered for simple nested attrsets.

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

```nix file:///flatten-attrs.nix
{ foo = { bar = 1; }; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///flatten-attrs.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":5
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The Quote action appears first, then Flatten action second.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "\"foo\""
     CHECK:       "title": "Quote attribute name"
     CHECK:       "newText": "foo.bar = 1;"
     CHECK:       "title": "Flatten nested attribute set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
