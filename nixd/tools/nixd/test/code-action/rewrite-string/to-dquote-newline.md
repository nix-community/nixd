# RUN: nixd --lit-test < %s | FileCheck %s

Test that newlines in indented strings are properly escaped when converting to double-quoted.

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

The indented string contains an actual newline character between hello and world.

```nix file:///to-dquote-newline.nix
{ x = ''hello
world''; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///to-dquote-newline.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":1,
            "character":7
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The newline becomes an escape sequence \n in the double-quoted string (shown as \\n in JSON).

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "\"hello\\nworld\""
     CHECK:       "title": "Convert to double-quoted string"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
