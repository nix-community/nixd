# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ nerd-fonts.fira-code.version = \"3.4.0\"; }" \
# RUN: < %s

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

```nix file:///basic.nix
with pkgs; [ nerd-fonts. ]
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/inlayHint",
    "params": {
        "textDocument":{
            "uri":"file:///basic.nix"
        },
        "range": {
          "start":{
            "line": 0,
            "character":0
          },
          "end":{
            "line":0,
            "character":26
          }
        }
    }
}
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
