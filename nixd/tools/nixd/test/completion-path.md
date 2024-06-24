# RUN: sed "s|ROOT|%S|g" < %s | nixd --lit-test | FileCheck %s

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
      },
      "trace":"off"
   }
}
```


<-- textDocument/didOpen


```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file://ROOT/completion-path-root/main.nix",
         "languageId":"nix",
         "version":1,
         "text":"{ bar = ./ }"
      }
   }
}
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file://ROOT/completion-path-root/main.nix"
        },
        "position": {
            "line": 0,
            "character": 9
        },
        "context": {
            "triggerKind": 1,
            "triggerCharacter": "/"
        }
    }
}
```

```
CHECK:      {
CHECK:        "data": "./",
CHECK:        "kind": 17,
CHECK:        "label": "foo",
CHECK:        "score": 0
CHECK:      },
CHECK:      {
CHECK:        "data": "./",
CHECK:        "kind": 17,
CHECK:        "label": "main.nix",
CHECK:        "score": 0
CHECK:      }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
