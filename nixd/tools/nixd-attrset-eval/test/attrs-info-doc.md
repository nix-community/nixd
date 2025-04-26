# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "{ hello = /** some markdown docs */x: y: x; }"
}
```


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/attrpathInfo",
   "params": [ "hello" ]
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "Meta": {
CHECK-NEXT:     "Location": null,
CHECK-NEXT:     "Type": 9
CHECK-NEXT:   },
CHECK-NEXT:   "PackageDesc": {
CHECK-NEXT:     "Description": null,
CHECK-NEXT:     "Homepage": null,
CHECK-NEXT:     "LongDescription": null,
CHECK-NEXT:     "Name": null,
CHECK-NEXT:     "PName": null,
CHECK-NEXT:     "Position": null,
CHECK-NEXT:     "Version": null
CHECK-NEXT:   },
CHECK-NEXT:   "ValueDesc": {
CHECK-NEXT:     "args": [],
CHECK-NEXT:     "arity": 0,
CHECK-NEXT:     "doc": "some markdown docs \n"
CHECK-NEXT:   }
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

