# Test that nixd reads the formatter's stdout while the formatter is still
# running. The formatter prints more than a pipe buffer of output, so nixd
# deadlocks if nixd waits for the formatter to exit before reading.

# RUN: timeout 60 nixd --lit-test -config='{ "formatting": { "command": ["%S/large-output-formatter"] } }' < %s | FileCheck %s

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
{ x = 1; }
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
            "insertSpaces": true
        }
    }
}
```

```
CHECK: "id": 2,
CHECK: "newText": "1\n2\n3\n{{.*}}\n19999\n20000\n",
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
