# RUN: mkdir -p %t.dir && echo '{ }' > %t.dir/target.nix && sed 's|TEST_DIR|%t.dir|g' %s > %t && nixd --lit-test < %t | FileCheck %s

Test go-to-definition for path expressions.
Verifies that path literals correctly resolve to target files.

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

```nix file://TEST_DIR/main.nix
./target.nix
```

<-- textDocument/definition(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file://TEST_DIR/main.nix"
      },
      "position":{
        "line": 0,
        "character": 2
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "range": {
CHECK-NEXT:     "end": {
CHECK-NEXT:       "character": 0,
CHECK-NEXT:       "line": 0
CHECK-NEXT:     },
CHECK-NEXT:     "start": {
CHECK-NEXT:       "character": 0,
CHECK-NEXT:       "line": 0
CHECK-NEXT:     }
CHECK-NEXT:   },
CHECK:        "uri": "file://
CHECK:        target.nix"
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
