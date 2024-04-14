# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ hello.meta.description = \"Very Nice\";  }" \
# RUN: < %s | FileCheck %s

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
CHECK-NEXT:      "value": "Very Nice\n\n"
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
