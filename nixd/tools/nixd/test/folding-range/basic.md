# RUN: nixd --lit-test < %s | FileCheck %s

Test basic folding range functionality for attribute sets and lists.

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
{
  # Multi-line attribute set
  attrs = {
    x = 1;
    y = 2;
  };

  # Multi-line list
  list = [
    1
    2
  ];
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
1. Outer attribute set (lines 0-12)
2. Inner "attrs" attribute set (lines 2-5)
3. List (lines 8-11)

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 1,
CHECK-NEXT:      "endLine": 12,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startLine": 0
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 3,
CHECK-NEXT:      "endLine": 5,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startCharacter": 10,
CHECK-NEXT:      "startLine": 2
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "endCharacter": 3,
CHECK-NEXT:      "endLine": 11,
CHECK-NEXT:      "kind": "region",
CHECK-NEXT:      "startCharacter": 9,
CHECK-NEXT:      "startLine": 8
CHECK-NEXT:    }
CHECK-NEXT:  ]
CHECK-NEXT:}
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
