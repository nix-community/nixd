# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s

```json
{
  "jsonrpc": "2.0",
  "id": 0,
  "method": "attrset/evalExpr",
  "params": "let mkAttrs = count: builtins.listToAttrs (builtins.genList (n: { name = \"item_${builtins.toString n}\"; value = { _type = \"option\"; }; }) count); in { attrs30 = mkAttrs 30; attrs31 = mkAttrs 30 // { item_zz = null; }; options30 = mkAttrs 30; options31 = mkAttrs 30 // { item_zz = throw \"the truncation sentinel must stay lazy\"; }; }"
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "attrset/attrpathComplete",
  "params": { "Scope": ["attrs30"], "Prefix": "item_" }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "IsIncomplete": false,
CHECK-NEXT:   "Items": [
CHECK-COUNT-30: "item_
CHECK-NEXT:   ]
CHECK-NEXT: }
```

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "attrset/attrpathComplete",
  "params": { "Scope": ["attrs31"], "Prefix": "item_" }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "IsIncomplete": true,
CHECK-NEXT:   "Items": [
CHECK-COUNT-30: "item_
CHECK-NEXT:   ]
CHECK-NEXT: }
```

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "attrset/optionComplete",
  "params": { "Scope": ["options30"], "Prefix": "item_" }
}
```

```
     CHECK: "id": 3,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "IsIncomplete": false,
CHECK-NEXT:   "Items": [
CHECK-COUNT-30: "Name": "item_
CHECK-NEXT:   }
CHECK-NEXT:   ]
CHECK-NEXT: }
```

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "attrset/optionComplete",
  "params": { "Scope": ["options31"], "Prefix": "item_" }
}
```

```
     CHECK: "id": 4,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "IsIncomplete": true,
CHECK-NEXT:   "Items": [
CHECK-COUNT-30: "Name": "item_
CHECK-NEXT:   }
CHECK-NEXT:   ]
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
