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


```nix
{ aaaaaaaaaaaaa
# ^~~~~~~~~~~~^
, bbbbbbbbbbbbb
# ^~~~~~~~~~~~^
, ccc
, ddd
}: {
  # ^~~~~~^
  formal1 = aaaaaaaaaaaaa;
  #             ^ <-- first request
  formal2 = bbbbbbbbbbbbb;
  #           ^ <--- second request
}
```

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///lambda-noarg.nix",
            "languageId": "nix",
            "version": 1,
            "text": "{ aaaaaaaaaaaaa\r\n# ^~~~~~~~~~~~^\r\n, bbbbbbbbbbbbb\r\n# ^~~~~~~~~~~~^\r\n, ccc\r\n, ddd\r\n}: {\r\n  # ^~~~~~^\r\n  formal1 = aaaaaaaaaaaaa;\r\n  #             ^ <-- first request\r\n  formal2 = bbbbbbbbbbbbb;\r\n  #           ^ <--- second request\r\n}"
        }
    }
}
```

<-- textDocument/definition(1)

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file:///lambda-noarg.nix"
      },
      "position":{
         "line":8,
         "character":17
      }
   }
}
```


```
     CHECK: "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 2,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 2,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
