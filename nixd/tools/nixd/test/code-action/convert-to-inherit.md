
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

## Test 1: Basic self-assignment { x = x; } -> { inherit x; }

<-- textDocument/didOpen

```nix file:///convert-to-inherit-basic.nix
{ x = x; }
```

<-- textDocument/codeAction(1)

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///convert-to-inherit-basic.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
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

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
     CHECK:       "newText": "inherit x;",
     CHECK:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Convert to inherit"
```

## Test 2: Simple select expression { a = b.a; } -> { inherit (b) a; }

<-- textDocument/didOpen

```nix file:///convert-to-inherit-select.nix
{ a = b.a; }
```

<-- textDocument/codeAction(2)

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///convert-to-inherit-select.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":9
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
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
     CHECK:       "newText": "inherit (b) a;",
     CHECK:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Convert to inherit"
```

## Test 3: Complex selector { a = b.c.a; } -> { inherit (b.c) a; }

<-- textDocument/didOpen

```nix file:///convert-to-inherit-complex.nix
{ a = b.c.a; }
```

<-- textDocument/codeAction(3)

```json
{
   "jsonrpc":"2.0",
   "id":3,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///convert-to-inherit-complex.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":11
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
     CHECK:   "id": 3,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
     CHECK:       "newText": "inherit (b.c) a;",
     CHECK:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Convert to inherit"
```

## Test 4: Negative case - non-matching names { x = y; }

<-- textDocument/didOpen

```nix file:///convert-to-inherit-negative.nix
{ x = y; }
```

<-- textDocument/codeAction(4)

```json
{
   "jsonrpc":"2.0",
   "id":4,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///convert-to-inherit-negative.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":3
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
     CHECK:   "id": 4,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NOT:       "title": "Convert to inherit"
```

## Test 5: Negative case - select expression with non-matching name { x = y.z; }

<-- textDocument/didOpen

```nix file:///convert-to-inherit-negative-select.nix
{ x = y.z; }
```

<-- textDocument/codeAction(5)

```json
{
   "jsonrpc":"2.0",
   "id":5,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///convert-to-inherit-negative-select.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":3
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
     CHECK:   "id": 5,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NOT:       "title": "Convert to inherit"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
