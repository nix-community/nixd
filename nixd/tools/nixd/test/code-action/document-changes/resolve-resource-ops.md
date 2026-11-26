# RUN: nixd --lit-test < %s | FileCheck %s

Test that `codeAction/resolve` round-trips a `WorkspaceEdit` whose
`documentChanges` contain every kind of resource operation.

Regression test for nixd#903: `DocumentChange::fromJSON` passed the whole
change object to the URI parser instead of the field value, so `create`,
`rename` and `delete` operations failed to parse and the enclosing `edit`
was silently dropped from the resolved action. nixd itself only emits
`create` operations today, so this test crafts the request directly to
cover the `rename` and `delete` branches as well.

Note: no `textDocument/didOpen` is needed — `codeAction/resolve` echoes the
given action.

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
         "workspace": {
            "workspaceEdit": {
               "documentChanges": true,
               "resourceOperations": ["create", "rename", "delete"]
            }
         }
      },
      "trace":"off"
   }
}
```


<-- codeAction/resolve(1)

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"codeAction/resolve",
   "params":{
      "edit":{
         "documentChanges":[
            {
               "kind":"create",
               "uri":"file:///created.nix"
            },
            {
               "kind":"rename",
               "oldUri":"file:///old.nix",
               "newUri":"file:///new.nix"
            },
            {
               "kind":"delete",
               "uri":"file:///gone.nix"
            },
            {
               "edits":[
                  {
                     "newText":"hello",
                     "range":{
                        "end":{
                           "character":0,
                           "line":0
                        },
                        "start":{
                           "character":0,
                           "line":0
                        }
                     }
                  }
               ],
               "textDocument":{
                  "uri":"file:///test.nix",
                  "version":null
               }
            }
         ]
      },
      "kind":"refactor",
      "title":"resource-op roundtrip"
   }
}
```

All four document changes survive the round-trip.

```
     CHECK:   "id": 1,
     CHECK:   "result": {
     CHECK:   "edit": {
     CHECK:     "documentChanges": [
     CHECK:       "kind": "create",
     CHECK:       "uri": "file:///created.nix"
     CHECK:       "kind": "rename",
     CHECK:       "newUri": "file:///new.nix"
     CHECK:       "oldUri": "file:///old.nix"
     CHECK:       "kind": "delete",
     CHECK:       "uri": "file:///gone.nix"
     CHECK:       "newText": "hello"
     CHECK:   "kind": "refactor",
     CHECK:   "title": "resource-op roundtrip"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```