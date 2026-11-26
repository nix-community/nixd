#!/bin/sh
# Print a lit test session that opens a Nix document larger than a pipe
# buffer (64 KiB on Linux) and asks nixd to format the document.
cat <<'END'
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

```nix file:///large.nix
[
END

seq 30000

cat <<'END'
]
```

<-- textDocument/formatting

```json
{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "textDocument/formatting",
    "params": {
        "textDocument": {
            "uri": "file:///large.nix"
        },
        "options": {
            "tabSize": 2,
            "insertSpaces": true
        }
    }
}
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
END
