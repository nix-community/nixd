# RUN: nixd --lit-test < %s | FileCheck %s

Test `with` to `let/inherit` conversion with nested `with` expressions.

When the cursor is on the outer `with`, the outer `with` is converted to `let/inherit`,
preserving the inner `with` in the body. The VLA tracks all unresolved variables
(`bar`, `foo`, `inner`) under the outer `with` scope's Definition, so they are all
inherited. Variables are sorted alphabetically.

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

```nix file:///with-to-let-nested.nix
with outer; with inner; foo bar
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-nested.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":4
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

```
     CHECK: "id": 2,
     CHECK: "newText": "let inherit (outer) bar foo inner; in with inner; foo bar",
     CHECK: "kind": "refactor.rewrite",
     CHECK: "title": "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
