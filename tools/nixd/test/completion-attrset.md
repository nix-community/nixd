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

Complete this file:

```nix
{
    some.long = 1;
    short = 2;
}
```

<-- textDocument/didOpen

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///foo.nix",
            "languageId": "nix",
            "version": 1,
            "text": "{\n    some.long = 1;\n    short = 2;\n}\n"
        }
    }
}
```

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didChange",
    "params": {
        "textDocument": {
            "uri": "file:///foo.nix",
            "version": 2
        },
        "contentChanges": [
            {
                "range": {
                    "start": {
                        "line": 3,
                        "character": 1
                    },
                    "end": {
                        "line": 3,
                        "character": 1
                    }
                },
                "rangeLength": 0,
                "text": "."
            }
        ]
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
            "uri": "file:///foo.nix"
        },
        "position": {
            "line": 3,
            "character": 2
        },
        "context": {
            "triggerKind": 2,
            "triggerCharacter": "."
        }
    }
}
```


--> reply:textDocument/completion(1)


```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "isIncomplete": false,
CHECK-NEXT:     "items": [
CHECK-NEXT:       {
CHECK-NEXT:         "kind": 5,
CHECK-NEXT:         "label": "some",
CHECK-NEXT:         "score": 0
CHECK-NEXT:       },
CHECK-NEXT:       {
CHECK-NEXT:         "kind": 5,
CHECK-NEXT:         "label": "short",
CHECK-NEXT:         "score": 0
CHECK-NEXT:       }
CHECK-NEXT:     ]
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
