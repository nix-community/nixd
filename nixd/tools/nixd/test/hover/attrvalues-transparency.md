# RUN: nixd --lit-test \
# RUN: --nixos-options-expr="{ services.openssh.enable = { _type = \"option\"; description = \"openssh option\"; type.description = \"bool\"; }; }" \
# RUN: < %s | FileCheck %s

# Cursor on `enable` inside `lib.mkMerge (lib.attrValues { ssh = MODULE; })`.
# Without the attrValues transparency in AST.cpp::getValueAttrPath, the
# attribute key `ssh` would be incorrectly prepended to the option path,
# producing the unresolvable `ssh.services.openssh.enable` and no hover.
# With it, the key is treated as transparent (since attrValues discards
# names) and the option resolves normally.

<-- initialize(0)

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"initialize",
   "params":{"processId":123,"rootPath":"","capabilities":{},"trace":"off"}
}
```


<-- textDocument/didOpen

```nix file:///test.nix
{ lib, ... }: lib.mkMerge (lib.attrValues {
  ssh = {
    services.openssh.enable = true;
  };
})
```


<-- textDocument/hover(2)

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/hover",
   "params":{
      "textDocument":{"uri":"file:///test.nix"},
      "position":{"line":2,"character":23}
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "contents": {
CHECK-NEXT:     "kind": "markdown",
CHECK-NEXT:     "value": " (bool)\n\nopenssh option"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
