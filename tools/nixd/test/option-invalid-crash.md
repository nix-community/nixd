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
        "workspace": {
            "configuration": true
        }
      },
      "trace":"off"
   }
}
```

<-- initialized

```json
{
   "jsonrpc":"2.0",
   "method":"initialized",
   "params":{

   }
}
```

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "result":[
      {
         "options": {
            "enable": true,
            "target": {
                "args": ["--expr", "aasd ?? ??? ?" ],
                "installable": ""
            }
         }
      }
   ]
}
```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///completion.nix",
         "languageId":"nix",
         "version":1,
         "text":"let\n  pkgs = {\n    a = 1;\n  };\nin\nwith pkgs;\n\na\n\n"
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
            "uri": "file:///completion.nix"
        },
        "position": {
            "line": 3,
            "character": 4
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```
```
CHECK: }
```


```

```json
{"jsonrpc":"2.0","method":"exit"}
```
