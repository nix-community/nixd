# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/builtinComplete",
   "params": {
        "Scope": [ ],
        "Prefix": ""
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": [
CHECK-NEXT:    "abort",
CHECK-NEXT:    "add",
CHECK-NEXT:    "addDrvOutputDependencies",
CHECK-NEXT:    "addErrorContext",
CHECK-NEXT:    "all",
CHECK-NEXT:    "any",
CHECK-NEXT:    "appendContext",
CHECK-NEXT:    "attrNames",
CHECK-NEXT:    "attrValues",
CHECK-NEXT:    "baseNameOf",
CHECK-NEXT:    "bitAnd",
CHECK-NEXT:    "bitOr",
CHECK-NEXT:    "bitXor",
CHECK-NEXT:    "break",
CHECK-NEXT:    "builtins",
CHECK-NEXT:    "catAttrs",
CHECK-NEXT:    "ceil",
CHECK-NEXT:    "compareVersions",
CHECK-NEXT:    "concatLists",
CHECK-NEXT:    "concatMap",
CHECK-NEXT:    "concatStringsSep",
CHECK-NEXT:    "convertHash",
CHECK-NEXT:    "currentSystem",
CHECK-NEXT:    "currentTime",
CHECK-NEXT:    "deepSeq",
CHECK-NEXT:    "derivation",
CHECK-NEXT:    "derivationStrict",
CHECK-NEXT:    "dirOf",
CHECK-NEXT:    "div",
CHECK-NEXT:    "elem",
CHECK-NEXT:    "elemAt"
CHECK-NEXT:  ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
