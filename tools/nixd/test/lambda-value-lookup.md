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


```nix
{ foo }:

{
  x = {
    y = {
      # Can we hover on "foo" ?
      # It should be associated to formal declaration.
      z = foo;
      #    ^ == "1"
    };
  };
}
```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///foo.nix",
         "languageId":"nix",
         "version":1,
         "text":"{ foo }:\r\n\r\n{\r\n  x = {\r\n    y = {\r\n      # Can we hover on \"foo\" ?\r\n      # It should be associated to formal declaration.\r\n      z = foo;\r\n      #    ^ == \"1\"\r\n    };\r\n  };\r\n}"
      }
   }
}
```


```nix
import ./foo.nix { foo = 1; }
```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///bar.nix",
         "languageId":"nix",
         "version":1,
         "text":"import ./foo.nix { foo = 1; }"
      }
   }
}
```

<-- textDocument/hover

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/hover",
   "params":{
      "textDocument":{
         "uri":"file:///foo.nix"
      },
      "position":{
         "line":7,
         "character":12
      }
   }
}
```

```
CHECK: "value": "## ExprVar \n Value: `1`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
