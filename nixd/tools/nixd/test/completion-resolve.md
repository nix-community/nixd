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
    "jsonrpc": "2.0",
    "id": 1,
    "method": "completionItem/resolve",
    "params": {
        "label": "hello",
        "data":  "{  \"Scope\": [ ], \"Prefix\": \"hel\" }"
    }
}
```

```
     CHECK:  "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "data": "{  \"Scope\": [ ], \"Prefix\": \"hel\" }",
CHECK-NEXT:    "detail": "{{.*}}",
CHECK-NEXT:    "documentation": {
CHECK-NEXT:      "kind": "markdown",
CHECK-NEXT:      "value": "A program that produces a familiar, friendly greeting\n\nGNU Hello is a program that prints \"Hello, world!\" when you run it.\nIt is fully customizable.\n"
CHECK-NEXT:    },
CHECK-NEXT:    "kind"
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
