# RUN: nixd --lit-test < %s | FileCheck %s

Test `add to formals` action on formals with a trailing comma before `}`,
e.g. `{ a, b, c, }:`. The trailing comma parses as a dangling-comma
placeholder `Formal` node (no identifier, not an ellipsis), which must not
be mistaken for the last *real* formal -- doing so would anchor the
insertion after the existing trailing comma and produce a double comma.

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

```nix file:///trailing-comma.nix
{a, b, c,}: d
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///trailing-comma.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 12
         },
         "end":{
            "line":0,
            "character": 13
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should insert `, d` right after `c` (character 8, the last real
formal), not after the trailing comma. Inserting at character 9 (after the
trailing comma, right before `}`) would produce `{a, b, c,, d}: d` -- a
double comma.

```
     CHECK: "id": 2,
     CHECK: "newText": ", d",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "character": 8,
     CHECK:     "line": 0
     CHECK:   "start":
     CHECK:     "character": 8,
     CHECK:     "line": 0
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "add `d` to formals"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
