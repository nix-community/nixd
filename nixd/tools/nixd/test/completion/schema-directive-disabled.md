# RUN: rm -rf %t && mkdir -p %t
# RUN: printf '{ pkgs, ... }: { options = { schemaBar = { inner = { _type = "option"; }; }; }; }\n' > %t/mod.nix
# RUN: sed "s|@DIR@|%t|g" %s > %t/in.md
# RUN: nixd --lit-test \
# RUN: --nixos-options-expr="{ foo.bar = { _type = \"option\"; }; }" \
# RUN: --nixpkgs-expr="{ lib = { mkDefault = x: x; evalModules = { modules }: (import (builtins.head modules)) (builtins.elemAt modules 1)._module.args; }; }" \
# RUN: < %t/in.md | FileCheck %s

Without `schemaDirective.enable`, a directive is an ordinary comment: nixd
evaluates nothing it names, and the workspace-configured options apply.

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


```nix file://@DIR@/config.nix
#:schema ./mod.nix
{ bar = 1; fo }
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file://@DIR@/config.nix"
        },
        "position": {
            "line": 1,
            "character": 12
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "isIncomplete": false,
CHECK-NEXT:    "items": [
CHECK-NEXT:      {
CHECK-NEXT:        "data": "",
CHECK-NEXT:        "detail": "nixos",
CHECK-NEXT:        "kind": 7,
CHECK-NEXT:        "label": "foo",
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
