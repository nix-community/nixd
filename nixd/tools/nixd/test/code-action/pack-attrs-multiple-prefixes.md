# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action works correctly when multiple independent prefixes exist.

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

```nix file:///pack-attrs-multiple-prefixes.nix
{ foo.a = 1; foo.b = 2; bar.c = 3; bar.d = 4; }
```

<-- textDocument/codeAction(2) - Test 'foo' prefix


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-multiple-prefixes.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":7
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Should offer actions only for 'foo' prefix, not 'bar'.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
```

Pack One for foo.a

```
     CHECK:       "newText": "foo = { a = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
```

Bulk pack for foo (not bar)

```
     CHECK:       "newText": "foo = { a = 1; b = 2; };"
     CHECK:       "title": "Pack all 'foo' bindings to nested set"
```

Recursive pack for foo

```
     CHECK:       "newText": "foo = { a = 1; b = 2; };"
     CHECK:       "title": "Recursively pack all 'foo' bindings to nested set"
```

<-- textDocument/codeAction(3) - Test 'bar' prefix


```json
{
   "jsonrpc":"2.0",
   "id":3,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-multiple-prefixes.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":24
         },
         "end":{
            "line":0,
            "character":29
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Should offer actions only for 'bar' prefix, not 'foo'.

```
     CHECK:   "id": 3,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
```

Pack One for bar.c

```
     CHECK:       "newText": "bar = { c = 3; };"
     CHECK:       "title": "Pack dotted path to nested set"
```

Bulk pack for bar

```
     CHECK:       "newText": "bar = { c = 3; d = 4; };"
     CHECK:       "title": "Pack all 'bar' bindings to nested set"
```

Recursive pack for bar

```
     CHECK:       "newText": "bar = { c = 3; d = 4; };"
     CHECK:       "title": "Recursively pack all 'bar' bindings to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
