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
        "workspace": {
            "configuration": true
        }
      },
      "trace":"off"
   }
}
```

<-- initialized

```json
{
   "jsonrpc":"2.0",
   "method":"initialized",
   "params":{

   }
}
```

This is a valid configuration.
The server should parse it correctly and do not crash.

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "result":[
      {
         "eval":{
            "target": {
               "args":[
                  "--file",
                  "/deeper.nix"
               ],
               "installable":""
            },
            "depth": 3
         }
      }
   ]
}
```
<-- textDocument/didOpen

```nix
{
  a = {
    b = 1;
    c = 2;
  };
  c = 1;
}
```


```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///deeper.nix",
            "languageId": "nix",
            "version": 1,
            "text": "{\n  a = {\n    b = 1;\n    c = 2;\n  };\n  c = 1;\n}\n"
        }
    }
}
```

<-- textDocument/hover(1)

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/hover",
   "params":{
      "textDocument":{
         "uri":"file:///deeper.nix"
      },
      "position":{
         "line":0,
         "character":0
      }
   }
}
```

```
CHECK: "value": "## ExprAttrs \n Value: `{ a = { b = 1; c = 2; }; c = 1; }`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
