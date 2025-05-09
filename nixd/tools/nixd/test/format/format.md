# RUN: nixd --lit-test -config='{ "formatting": { "command": ["%S/nixfmt"] } }' < %s | FileCheck %s

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

```nix file:///format.nix
{ stdenv,
pkgs}: 
 let x=1; in { y = x; }
```

<-- textDocument/formatting

```json
{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "textDocument/formatting",
    "params": {
        "textDocument": {
            "uri": "file:///format.nix"
        },
        "options": {
            "tabSize": 2,
            "insertSpaces": true,
            "trimTrailingWhitespace": true,
            "insertFinalNewline": true
        }
    }
}
```

```
CHECK: "newText": "Hello\n",
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
