# RUN: nixd --lit-test < %s > %t
# RUN: FileCheck %s < %t
# RUN: FileCheck %s --check-prefix=ABSENT < %t

Do not offer single-binding Pack for incomplete bindings. In particular, an
inner semicolon must not be mistaken for the binding's own terminator.

ABSENT-NOT: "title": "Pack dotted path to nested set"

```json
{
  "jsonrpc": "2.0",
  "id": 0,
  "method": "initialize",
  "params": {
    "processId": 123,
    "rootPath": "",
    "capabilities": {}
  }
}
```

```nix file:///pack-attrs-incomplete-2.nix
{ a.b = 1 }
```

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-incomplete-2.nix"
    },
    "range": {
      "start": {
        "line": 0,
        "character": 2
      },
      "end": {
        "line": 0,
        "character": 2
      }
    },
    "context": {
      "diagnostics": []
    }
  }
}
```

```
CHECK-DAG: "id": 2,
```

```nix file:///pack-attrs-incomplete-3.nix
{ a.b = ; }
```

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-incomplete-3.nix"
    },
    "range": {
      "start": {
        "line": 0,
        "character": 2
      },
      "end": {
        "line": 0,
        "character": 2
      }
    },
    "context": {
      "diagnostics": []
    }
  }
}
```

```
CHECK-DAG: "id": 3,
```

```nix file:///pack-attrs-incomplete-4.nix
{ a.b 1; }
```

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-incomplete-4.nix"
    },
    "range": {
      "start": {
        "line": 0,
        "character": 2
      },
      "end": {
        "line": 0,
        "character": 2
      }
    },
    "context": {
      "diagnostics": []
    }
  }
}
```

```
CHECK-DAG: "id": 4,
```

```nix file:///pack-attrs-incomplete-5.nix
{ a.b = { x = 1; } }
```

```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-incomplete-5.nix"
    },
    "range": {
      "start": {
        "line": 0,
        "character": 2
      },
      "end": {
        "line": 0,
        "character": 2
      }
    },
    "context": {
      "diagnostics": []
    }
  }
}
```

```
CHECK-DAG: "id": 5,
```

```nix file:///pack-attrs-incomplete-6.nix
{ a.b = { x = 1; }; a.c = 2 }
```

```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-incomplete-6.nix"
    },
    "range": {
      "start": {
        "line": 0,
        "character": 20
      },
      "end": {
        "line": 0,
        "character": 20
      }
    },
    "context": {
      "diagnostics": []
    }
  }
}
```

```
CHECK-DAG: "id": 6,
```

```json
{
  "jsonrpc": "2.0",
  "method": "exit"
}
```
