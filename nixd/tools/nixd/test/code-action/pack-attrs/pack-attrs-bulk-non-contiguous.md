# RUN: nixd --lit-test < %s | FileCheck %s

Test that bulk Pack action correctly handles non-contiguous sibling bindings with the same prefix.

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

```nix file:///pack-attrs-bulk-non-contiguous.nix
{ foo.a = 1; bar = 2; foo.b = 3; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-bulk-non-contiguous.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":8
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Three Pack actions should be offered:
1. Pack One - only the current binding (foo.a)
2. Shallow Pack All - all 'foo' bindings (foo.a and foo.b), even though they are separated by 'bar'
3. Recursive Pack All - all 'foo' bindings, fully nested

For the bulk actions, the WorkspaceEdit must contain two TextEdits: one that
replaces the first `foo.*` binding with the packed result, and one that deletes
the second `foo.*` binding in place. The intermediate `bar = 2;` binding is
left untouched in the source.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
CHECK-NEXT:     {
```

Action 1: Pack One - only foo.a, single edit replacing offset 2..12.

```
     CHECK:       "newText": "foo = { a = 1; };"
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 12,
     CHECK:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 2: Shallow Pack All - replaces foo.a with the packed text, then deletes foo.b. `bar = 2;` is preserved.

```
     CHECK:       "newText": "foo = { a = 1; b = 3; };"
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 12,
     CHECK:       "newText": ""
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 32,
     CHECK:           "character": 22,
     CHECK:       "title": "Pack all 'foo' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 3: Recursive Pack All - same shape (single-level path), two edits.

```
     CHECK:       "newText": "foo = { a = 1; b = 3; };"
CHECK-NEXT:       "range": {
     CHECK:       "newText": ""
CHECK-NEXT:       "range": {
CHECK-NEXT:         "end": {
CHECK-NEXT:           "character": 32,
     CHECK:       "title": "Recursively pack all 'foo' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
