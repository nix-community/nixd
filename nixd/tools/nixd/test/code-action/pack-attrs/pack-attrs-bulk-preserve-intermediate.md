# RUN: nixd --lit-test < %s | FileCheck %s

Regression test for the bug where "Pack all" bulk actions silently dropped
non-matching bindings sitting between matching siblings. With input

```
{
  a.x = 1;
  b.y = 2;
  a.z = 3;
}
```

the action incorrectly replaced the whole `a.x = 1; ... a.z = 3;` span with
the packed `a = { x = 1; z = 3; };`, deleting `b.y = 2;`.

The fix emits a multi-edit WorkspaceEdit:
- replace the first matching binding with the full packed result,
- delete every subsequent matching binding in place,
- leave intermediate non-matching bindings untouched.

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

```nix file:///pack-attrs-bulk-preserve-intermediate.nix
{
  a.x = 1;
  b.y = 2;
  a.z = 3;
}
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-bulk-preserve-intermediate.nix"
      },
      "range":{
         "start":{
            "line": 1,
            "character":2
         },
         "end":{
            "line":1,
            "character":5
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Action 1: Pack One - replaces just `a.x = 1;` on line 1.

```
     CHECK:   "result": [
     CHECK:       "newText": "a = { x = 1; };"
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 10,
CHECK-NEXT:           "line": 1
     CHECK:       "title": "Pack dotted path to nested set"
```

Action 2: Shallow Pack All - replaces `a.x = 1;` on line 1 with the packed
result and deletes `a.z = 3;` on line 3. `b.y = 2;` on line 2 is preserved.

```
     CHECK:       "newText": "a = { x = 1; z = 3; };"
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 10,
CHECK-NEXT:           "line": 1
     CHECK:       "newText": ""
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 10,
CHECK-NEXT:           "line": 3
     CHECK:           "character": 2,
CHECK-NEXT:           "line": 3
     CHECK:       "title": "Pack all 'a' bindings to nested set"
```

Action 3: Recursive Pack All - same two-edit shape for this single-level case.

```
     CHECK:       "newText": "a = { x = 1; z = 3; };"
CHECK-NEXT:       "range": {
     CHECK:       "newText": ""
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 10,
CHECK-NEXT:           "line": 3
     CHECK:       "title": "Recursively pack all 'a' bindings to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
