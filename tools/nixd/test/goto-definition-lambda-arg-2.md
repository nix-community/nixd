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
} @ aaaaarg : {
  # ^~~~~~^
  formal1 = aaaaaaaaaaaaa;
  #             ^ <-- first request
  formal2 = bbbbbbbbbbbbb;
  #           ^ <--- second request
  arg = aaaaarg;
  #      ^ <-- last request
}
```

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///lambda-arg.nix",
            "languageId": "nix",
            "version": 1,
            "text": "{ aaaaaaaaaaaaa\r\n# ^~~~~~~~~~~~^\r\n, bbbbbbbbbbbbb\r\n# ^~~~~~~~~~~~^\r\n, ccc\r\n, ddd\r\n} @ aaaaarg : {\r\n  # ^~~~~~^\r\n  formal1 = aaaaaaaaaaaaa;\r\n  #             ^ <-- first request\r\n  formal2 = bbbbbbbbbbbbb;\r\n  #           ^ <--- second request\r\n  arg = aaaaarg;\r\n  #      ^ <-- last request\r\n}"
        }
    }
}
```

<-- textDocument/definition(2)

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file:///lambda-arg.nix"
      },
      "position":{
         "line":10,
         "character":15
      }
   }
}
```
```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 2,
CHECK-NEXT:         "line": 2
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 2,
CHECK-NEXT:         "line": 2
CHECK-NEXT:       }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
