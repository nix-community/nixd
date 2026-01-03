
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

## Test 1: Basic inherit without expression
## { inherit x; } -> { x = x; }

<-- textDocument/didOpen

```nix file:///basic-inherit.nix
{ inherit x; }
```

<-- textDocument/codeAction(1)

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///basic-inherit.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":10
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
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
     CHECK:       "newText": "x = x;",
     CHECK:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Convert to explicit binding"
```

## Test 2: Inherit with expression
## { inherit (b) a; } -> { a = b.a; }

<-- textDocument/didOpen

```nix file:///inherit-with-expr.nix
{ inherit (b) a; }
```

<-- textDocument/codeAction(2)

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///inherit-with-expr.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":14
         },
         "end":{
            "line":0,
            "character":15
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
     CHECK:       "newText": "a = b.a;",
     CHECK:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Convert to explicit binding"
```

## Test 3: Inherit with complex expression
## { inherit (pkgs.lib) mkOption; } -> { mkOption = pkgs.lib.mkOption; }

<-- textDocument/didOpen

```nix file:///inherit-complex-expr.nix
{ inherit (pkgs.lib) mkOption; }
```

<-- textDocument/codeAction(3)

```json
{
   "jsonrpc":"2.0",
   "id":3,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///inherit-complex-expr.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":21
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

```
     CHECK:   "id": 3,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
     CHECK:       "newText": "mkOption = pkgs.lib.mkOption;",
     CHECK:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Convert to explicit binding"
```

## Test 4: Negative case - multiple names
## { inherit x y; } should NOT offer the action (only works with single name)

<-- textDocument/didOpen

```nix file:///multi-inherit.nix
{ inherit x y; }
```

<-- textDocument/codeAction(4)

```json
{
   "jsonrpc":"2.0",
   "id":4,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///multi-inherit.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":10
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
     CHECK:   "id": 4,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NOT:       "title": "Convert to explicit binding"
```

## Test 5: Inherit with longer identifier name
## { inherit fooBar; } -> { fooBar = fooBar; }

<-- textDocument/didOpen

```nix file:///inherit-long-name.nix
{ inherit fooBar; }
```

<-- textDocument/codeAction(5)

```json
{
   "jsonrpc":"2.0",
   "id":5,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///inherit-long-name.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":10
         },
         "end":{
            "line":0,
            "character":16
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
     CHECK:       "newText": "fooBar = fooBar;",
     CHECK:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Convert to explicit binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
