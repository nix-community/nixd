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


<-- textDocument/didOpen

```nix file:///basic.nix
/*
```

```
     CHECK:  "method": "textDocument/publishDiagnostics",
CHECK-NEXT:  "params": {
CHECK-NEXT:    "diagnostics": [
CHECK-NEXT:      {
CHECK-NEXT:        "code": "lex-unterminated-bcomment",
CHECK-NEXT:        "message": "unterminated /* comment (fix available)",
CHECK-NEXT:        "range": {
CHECK-NEXT:          "end": {
CHECK-NEXT:            "character": 2,
CHECK-NEXT:            "line": 0
CHECK-NEXT:          },
CHECK-NEXT:          "start": {
CHECK-NEXT:            "character": 2,
CHECK-NEXT:            "line": 0
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        "relatedInformation": [
CHECK-NEXT:          {
CHECK-NEXT:            "location": {
CHECK-NEXT:              "range": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 2,
CHECK-NEXT:                  "line": 0
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 0,
CHECK-NEXT:                  "line": 0
CHECK-NEXT:                }
CHECK-NEXT:              },
CHECK-NEXT:              "uri": "file:///basic.nix"
CHECK-NEXT:            },
CHECK-NEXT:            "message": "/* comment begins at here"
CHECK-NEXT:          }
CHECK-NEXT:        ],
CHECK-NEXT:        "severity": 1,
CHECK-NEXT:        "source": "nixf"
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "code": "note-bcomment-begin",
CHECK-NEXT:        "message": "/* comment begins at here",
CHECK-NEXT:        "range": {
CHECK-NEXT:          "end": {
CHECK-NEXT:            "character": 2,
CHECK-NEXT:            "line": 0
CHECK-NEXT:          },
CHECK-NEXT:          "start": {
CHECK-NEXT:            "character": 0,
CHECK-NEXT:            "line": 0
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        "relatedInformation": [
CHECK-NEXT:          {
CHECK-NEXT:            "location": {
CHECK-NEXT:              "range": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 2,
CHECK-NEXT:                  "line": 0
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 2,
CHECK-NEXT:                  "line": 0
CHECK-NEXT:                }
CHECK-NEXT:              },
CHECK-NEXT:              "uri": "file:///basic.nix"
CHECK-NEXT:            },
CHECK-NEXT:            "message": "original diagnostic"
CHECK-NEXT:          }
CHECK-NEXT:        ],
CHECK-NEXT:        "severity": 4,
CHECK-NEXT:        "source": "nixf"
CHECK-NEXT:      }
CHECK-NEXT:    ],
CHECK-NEXT:    "uri": "file:///basic.nix",
CHECK-NEXT:    "version": 1
```



```json
{"jsonrpc":"2.0","method":"exit"}
```
