# RUN: nixd --lit-test \
# RUN: --nixos-options-expr="{ foo.bar = { _type = \"option\"; type = { name = \"bool\"; description = \"boolean\"; }; defaultText = { _type = \"literalExpression\"; text = \"pkgs.stdenv.hostPlatform.isLinux\"; }; }; }" \
# RUN: < %s | FileCheck %s

<-- initialize(0)

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"initialize",
   "params":{
      "processId":123,
      "rootPath":"",
      "capabilities": {
        "textDocument": {
          "completion": {
            "completionItem": {
              "snippetSupport": true
            }
          }
        }
      },
      "trace":"off"
   }
}
```


<-- textDocument/didOpen


```nix file:///completion.nix
{ bar = 1; foo.ba }
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file:///completion.nix"
        },
        "position": {
            "line": 0,
            "character": 16
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

When an option has only a `defaultText` (no `example`), the completion
label should be the plain option name without any `(default)` suffix.

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "isIncomplete": false,
CHECK-NEXT:   "items": [
CHECK-NEXT:     {
CHECK-NEXT:       "data": "",
CHECK-NEXT:       "detail": "nixos | bool (boolean)",
CHECK-NEXT:       "documentation": null,
CHECK-NEXT:       "filterText": "bar",
CHECK-NEXT:       "insertText": "bar = ${1:pkgs.stdenv.hostPlatform.isLinux};",
CHECK-NEXT:       "insertTextFormat": 2,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "bar",
CHECK-NEXT:       "score": 0,
CHECK-NEXT:       "sortText": "1bar"
CHECK-NEXT:     }
CHECK-NEXT:   ]
CHECK-NEXT: }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
