# RUN: nixd --lit-test < %s | FileCheck %s

A quoted name with a raw CRLF in it: Nix reads it as LF, and the binding writes the name as written, not re-escaped as `\r\n`.

<-- initialize(0)

```json
{
   "jsonrpc": "2.0",
   "id": 0,
   "method": "initialize",
   "params": {
      "processId": 123,
      "rootPath": "",
      "capabilities": {},
      "trace": "off"
   }
}
```


<-- textDocument/didOpen

```nix file:///crlf.nix
let e = { "a\nb" = 1; z = 2; }; in { inherit (e) "a
b" z; }
```

<-- textDocument/codeAction(2)

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///crlf.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 50
         },
         "end": {
            "line": 0,
            "character": 50
         }
      },
      "context": {
         "diagnostics": [],
         "triggerKind": 2
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
     CHECK: "newText": "",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 2,
CHECK-NEXT: "line": 1
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 48,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": " \"a\r\nb\" = e.\"a\r\nb\";",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 5,
CHECK-NEXT: "line": 1
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 5,
CHECK-NEXT: "line": 1
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: ]
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "kind": "refactor.rewrite",
CHECK-NEXT: "title": "Convert to explicit binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
