# RUN: nixd --lit-test < %s | FileCheck %s

Test that the code action IS offered for the innermost `with` in an indirect
nesting scenario.

Even when there's indirect nesting, the innermost `with` is safe to convert
because its variables cannot come from an outer `with` scope.

See: https://github.com/nix-community/nixd/pull/768#discussion_r2681198142

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

```nix file:///with-to-let-indirect-inner-ok.nix
with outer; let x = with inner; foo; in x
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-indirect-inner-ok.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":20
         },
         "end":{
            "line":0,
            "character":24
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should be offered for the inner `with` because it's the innermost
scope for the variable `foo`. Converting it to `let/inherit` is safe.

```
     CHECK: "id": 2,
     CHECK: "newText": "let inherit (inner) foo; in foo",
     CHECK: "kind": "refactor.rewrite",
     CHECK: "title": "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
