# RUN: nixd --lit-test < %s | FileCheck %s

Test folding range for let expressions and lambda functions.

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

```nix file:///test.nix
let
  x = 1;
  y = 2;
in
{
  fn = { pkgs, lib, ... }:
  {
    name = "test";
  };
}
```

<-- textDocument/foldingRange(2)

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/foldingRange",
   "params":{
      "textDocument":{
         "uri":"file:///test.nix"
      }
   }
}
```

We expect folding ranges for:
1. Let expression (lines 0-9)
2. Let bindings (lines 1-2)
3. Attribute set in let body (lines 4-9)
4. Lambda expression (lines 5-8)
5. Inner attribute set in lambda body (lines 6-8)

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 1,
CHECK-NEXT:      "endLine": 9,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startLine": 0
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 8,
CHECK-NEXT:      "endLine": 2,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startCharacter": 2,
CHECK-NEXT:      "startLine": 1
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 1,
CHECK-NEXT:      "endLine": 9,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startLine": 4
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 3,
CHECK-NEXT:      "endLine": 8,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startCharacter": 7,
CHECK-NEXT:      "startLine": 5
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 3,
CHECK-NEXT:      "endLine": 8,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startCharacter": 2,
CHECK-NEXT:      "startLine": 6
CHECK-NEXT:    }
CHECK-NEXT:  ]
CHECK-NEXT:}
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

