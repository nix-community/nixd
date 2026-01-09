# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action correctly handles quoted inner (non-first) path segments.
Unlike pack-attrs-quoted.md which tests quoted FIRST segments ("foo-bar".baz),
this tests quoted INNER segments (foo."bar-baz").
This pattern is used in NixOS for attributes with special characters like dots
(e.g., `boot.kernel.sysctl."net.ipv4.tcp_keepalive_time"`).

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

```nix file:///pack-attrs-quoted-inner.nix
{ foo."bar-baz" = 1; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-quoted-inner.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":14
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Pack action should preserve the quoted inner segment "bar-baz".

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "foo = { \"bar-baz\" = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
