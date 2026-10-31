# RUN: nixd --lit-test < %s | FileCheck %s

Test that the "Extract to file" action survives the `codeAction/resolve`
round-trip with its `edit` intact.

Regression test for nixd#903: `DocumentChange::fromJSON` failed to parse
resource operations (`create`/`rename`/`delete`) because the whole change
object was passed to the URI parser instead of the field value, and
`CodeAction::fromJSON` silently dropped the failed `edit` parse. As a result
`codeAction/resolve` echoed actions without their `edit` field, and clients
applied nothing.

The client resolves the exact action the server offered. The two handlers
run on a thread pool, so the two replies may arrive in either order; the
CHECKs below are order-agnostic. The load-bearing assertion is the doubled
`CHECK-DAG` of the extract action's `newText`: it appears once inside the
`codeAction` result and once more only if the resolve reply carried the
`edit` back. With the regression unpatched it appears a single time and the
test fails.

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


<-- textDocument/didOpen

```nix file:///test.nix
{ services.awawa = { enable = true; settings = {}; }; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///test.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 19
         },
         "end":{
            "line":0,
            "character": 19
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The extract action is offered for the attribute set value of `services.awawa`,
and the client resolves it verbatim.

<-- codeAction/resolve(3)

```json
{
   "jsonrpc":"2.0",
   "id":3,
   "method":"codeAction/resolve",
   "params":{
      "edit":{
         "documentChanges":[
            {
               "kind":"create",
               "options":{
                  "ignoreIfExists":false,
                  "overwrite":false
               },
               "uri":"file:///awawa.nix"
            },
            {
               "edits":[
                  {
                     "newText":"{ enable = true; settings = {}; }\n",
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
                  "uri":"file:///awawa.nix",
                  "version":0
               }
            },
            {
               "edits":[
                  {
                     "newText":"import ./awawa.nix",
                     "range":{
                        "end":{
                           "character":52,
                           "line":0
                        },
                        "start":{
                           "character":19,
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
      "title":"Extract to awawa.nix"
   }
}
```

Both replies are present; the extract action's `newText` appears twice —
once in the `codeAction` result and once more only if the resolved action
carried its `edit`.

```
     CHECK-DAG:   "id": 2
     CHECK-DAG:   "id": 3
     CHECK-DAG:   "newText": "import ./awawa.nix"
     CHECK-DAG:   "newText": "import ./awawa.nix"
     CHECK-DAG:   "title": "Extract to awawa.nix"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```