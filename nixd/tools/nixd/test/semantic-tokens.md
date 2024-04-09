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
{
    x = 1;
    anonymousLambda = { a }: a;
    namedLambda = a: a;
    numbers = 1 + 2.0;
    bool = true;
    bool2= false;
    string = "1";
    string2 = "${builtins.foo}";
    undefined = x;
    list = [ 1 2 3 ];
    null = null;
    wit = with builtins; [ a b c];
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
         "text":"{\n    x = 1;\n    anonymousLambda = { a }: a;\n    namedLambda = a: a;\n    numbers = 1 + 2.0;\n    bool = true;\n    bool2= false;\n    string = \"1\";\n    string2 = \"${builtins.foo}\";\n    undefined = x;\n    list = [ 1 2 3 ];\n}\n"
      }
   }
}
```

<-- textDocument/semanticTokens(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/semanticTokens/full",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      }
   }
}
```

```
     CHECK:    "id": 2,
CHECK-NEXT:    "jsonrpc": "2.0",
CHECK-NEXT:    "result": {
CHECK-NEXT:      "data": [
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        1,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        15,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        20,
CHECK-NEXT:        1,
CHECK-NEXT:        12,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        5,
CHECK-NEXT:        1,
CHECK-NEXT:        5,
CHECK-NEXT:        0,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        11,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        14,
CHECK-NEXT:        1,
CHECK-NEXT:        11,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        3,
CHECK-NEXT:        1,
CHECK-NEXT:        5,
CHECK-NEXT:        0,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        7,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        4,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        7,
CHECK-NEXT:        4,
CHECK-NEXT:        9,
CHECK-NEXT:        1,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        5,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        7,
CHECK-NEXT:        5,
CHECK-NEXT:        9,
CHECK-NEXT:        1,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        6,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        9,
CHECK-NEXT:        3,
CHECK-NEXT:        1,
CHECK-NEXT:        0,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        7,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        9,
CHECK-NEXT:        10,
CHECK-NEXT:        0,
CHECK-NEXT:        0,
CHECK-NEXT:        12,
CHECK-NEXT:        1,
CHECK-NEXT:        5,
CHECK-NEXT:        2,
CHECK-NEXT:        1,
CHECK-NEXT:        4,
CHECK-NEXT:        4,
CHECK-NEXT:        10,
CHECK-NEXT:        0
CHECK-NEXT:      ],
CHECK-NEXT:      "resultId": ""
CHECK-NEXT:    }
CHECK-NEXT:  }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
