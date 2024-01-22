# RUN: nixd-next --lit-test < %s | FileCheck %s

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

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"/*"
      }
   }
}
```

```
     CHECK:  "method": "textDocument/publishDiagnostics",
CHECK-NEXT:  "params": {
CHECK-NEXT:    "diagnostics": [
CHECK-NEXT:      {
CHECK-NEXT:        "code": "lex-unterminated-bcomment",
CHECK-NEXT:        "message": "unterminated /* comment",
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
CHECK-NEXT:        "severity": 1,
CHECK-NEXT:        "source": "nixf"
CHECK-NEXT:      },
CHECK-NEXT:      {
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
CHECK-NEXT:        "severity": 3,
CHECK-NEXT:        "source": "nixf"
CHECK-NEXT:      }
CHECK-NEXT:    ],
CHECK-NEXT:    "uri": "file:///basic.nix",
```



```json
{"jsonrpc":"2.0","method":"exit"}
```
