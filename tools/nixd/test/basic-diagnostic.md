# RUN: nixd --lit-test < %s | FileCheck %s

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

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/didOpen",
  "params": {
    "textDocument": {
      "uri": "file:///basic.nix",
      "languageId": "nix",
      "version": 1,
      "text": "let x = 1; in x"
    }
  }
}
```

Expected no diagnostics for this file:

```
     CHECK:   "method": "textDocument/publishDiagnostics",
CHECK-NEXT:   "params": {
CHECK-NEXT:     "diagnostics": [],
CHECK-NEXT:     "uri": "file:///basic.nix",
CHECK-NEXT:     "version": 1
CHECK-NEXT:   }
CHECK-NEXT: }
```

<-- textDocument/didOpen

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/didOpen",
  "params": {
    "textDocument": {
      "uri": "file:///basic.nix",
      "languageId": "nix",
      "version": 1,
      "text": "what ? x not parsed!"
    }
  }
}
```

```
     CHECK:         "message": "syntax error, unexpected ID, expecting end of file",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 9,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 9,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         },
CHECK-NEXT:         "severity": 1
CHECK-NEXT:       }
CHECK-NEXT:     ],
CHECK-NEXT:     "uri": "file:///basic.nix",
CHECK-NEXT:     "version": 1
CHECK-NEXT:   }
CHECK-NEXT: }
```

Then, check that we do eval the file. Nix files are legal expressions, and should be evaluable.

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/didOpen",
  "params": {
    "textDocument": {
      "uri": "file:///test-eval.nix",
      "languageId": "nix",
      "version": 1,
      "text": "let x = 1; in y"
    }
  }
}
```

```
     CHECK:         "message": "undefined variable 'y'",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 14,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 14,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         },
CHECK-NEXT:         "severity": 1
CHECK-NEXT:       }
CHECK-NEXT:     ],
```

Check that we handle relative paths corretly. Introduce `importee.nix` and `importor.nix` in our workspace, and test whether `importor.nix` can **import** `importee.nix`.

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/didOpen",
  "params": {
    "textDocument": {
      "uri": "file:///importee.nix",
      "languageId": "nix",
      "version": 1,
      "text": "{a, b}: a + b"
    }
  }
}
```

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/didOpen",
  "params": {
    "textDocument": {
      "uri": "file:///importor.nix",
      "languageId": "nix",
      "version": 1,
      "text": "import ./importee.nix { a = 1; }"
    }
  }
}
```

```
CHECK: "message": "function 'anonymous lambda' called without required argument 'b'",
```

```json
{ "jsonrpc": "2.0", "method": "exit" }
```
