# RUN: nixd --lit-test \
# RUN: --nixos-options-expr="{ foo.bar = { _type = \"option\"; type = { name = \"bool\"; description = \"boolean\"; }; example = { _type = \"literalExpression\"; text = \"true\"; }; defaultText = { _type = \"literalExpression\"; text = \"pkgs.stdenv.hostPlatform.isLinux\"; }; }; }" \
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

An option with both `example` and `defaultText` (both wrapped in
`literalExpression`, as is conventional in nixpkgs) is offered as two
completion items so the user can pick which value to insert. The
`example` variant sorts first (sortText prefix "0") and the `default`
variant second ("1").

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
CHECK-NEXT:       "insertText": "bar = ${1:true};",
CHECK-NEXT:       "insertTextFormat": 2,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "bar (example)",
CHECK-NEXT:       "score": 0,
CHECK-NEXT:       "sortText": "0bar"
CHECK-NEXT:     },
CHECK-NEXT:     {
CHECK-NEXT:       "data": "",
CHECK-NEXT:       "detail": "nixos | bool (boolean)",
CHECK-NEXT:       "documentation": null,
CHECK-NEXT:       "filterText": "bar",
CHECK-NEXT:       "insertText": "bar = ${1:pkgs.stdenv.hostPlatform.isLinux};",
CHECK-NEXT:       "insertTextFormat": 2,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "bar (default)",
CHECK-NEXT:       "score": 0,
CHECK-NEXT:       "sortText": "1bar"
CHECK-NEXT:     }
CHECK-NEXT:   ]
CHECK-NEXT: }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
