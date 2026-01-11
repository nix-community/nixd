# RUN: nixd --lit-test < %s | FileCheck %s

Test `with` to `let/inherit` conversion with variable shadowing.

When a variable from the `with` scope is shadowed by a local binding,
it should NOT be included in the inherit list.

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

```nix file:///with-to-let-shadowing.nix
with lib; let foo = 1; in foo + optionalString true "x"
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-shadowing.nix"
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

The action should convert `with lib; let foo = 1; in foo + optionalString true "x"` to
`let inherit (lib) optionalString; in let foo = 1; in foo + optionalString true "x"`

The variable `foo` is shadowed by the inner let binding, so only `optionalString`
should be inherited from `lib`.

```
     CHECK: "id": 2,
     CHECK: "newText": "let inherit (lib) optionalString; in let foo = 1; in foo + optionalString true \"x\"",
     CHECK: "kind": "refactor.rewrite",
     CHECK: "title": "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
