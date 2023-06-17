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
let
    add = { foo, bar }: foo + bar;
in
add {
    # <-- complete me! expected: "foo", "bar"
}
```

<-- textDocument/didOpen

```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///foo.nix",
            "languageId": "nix",
            "version": 1,
            "text": "let\r\n    add = { foo, bar }: foo + bar;\r\nin\r\nadd {\r\n    # <-- complete me! expected: \"foo\", \"bar\"\r\n}"
        }
    }
}
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file:///foo.nix"
        },
        "position": {
            "line": 4,
            "character": 0
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```


--> reply:textDocument/completion(1)


```
     CHECK:   "id": 1,
     CHECK:         "kind": 4,
     CHECK:         "label": "foo",
     CHECK:         "score": 0
     CHECK:         "kind": 4,
     CHECK:         "label": "bar",
     CHECK:         "score": 0
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
