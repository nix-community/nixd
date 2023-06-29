# RUN: nixd --lit-test < %s | FileCheck %s

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

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///format.nix",
         "languageId":"nix",
         "version":1,
         "text":"{ stdenv,\npkgs}: \n let x=1; in { y = x; }"
      }
   }
}
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
CHECK: "newText": "{ stdenv\n, pkgs\n}:\nlet x = 1; in { y = x; }\n",
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
