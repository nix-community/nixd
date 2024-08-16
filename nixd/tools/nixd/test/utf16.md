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
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"{ a = 1; 测试文本 }"
      }
   }
}
```

```
     CHECK:  "code": "parse-unexpected",
CHECK-NEXT:  "message": "unexpected text (fix available)",
CHECK-NEXT:  "range": {
CHECK-NEXT:    "end": {
CHECK-NEXT:      "character": 13,
CHECK-NEXT:      "line": 0
CHECK-NEXT:    },
CHECK-NEXT:    "start": {
CHECK-NEXT:      "character": 9,
CHECK-NEXT:      "line": 0
CHECK-NEXT:    }
CHECK-NEXT:  },
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
