# RUN: nixd --lit-test < %s > %t
# RUN: FileCheck %s < %t
# RUN: FileCheck %s --check-prefix=ABSENT < %t

ABSENT-NOT: Flatten nested attribute set

Responses may arrive out of order; check every response ID without imposing an
order, and reject Flatten anywhere in the output.

Do not offer Flatten when the outer or inner binding lacks a required delimiter or value.

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

```nix file:///flatten-attrs-incomplete-2.nix
{ a = { b = 1; } }
```

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-incomplete-2.nix"
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
      "diagnostics": [],
      "triggerKind": 2
    }
  }
}
```

```
CHECK-DAG: "id": 2,
```

```nix file:///flatten-attrs-incomplete-3.nix
{ a = { b = 1 }; }
```

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-incomplete-3.nix"
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
      "diagnostics": [],
      "triggerKind": 2
    }
  }
}
```

```
CHECK-DAG: "id": 3,
```

```nix file:///flatten-attrs-incomplete-4.nix
{ a = { b = ; }; }
```

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-incomplete-4.nix"
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
      "diagnostics": [],
      "triggerKind": 2
    }
  }
}
```

```
CHECK-DAG: "id": 4,
```

```nix file:///flatten-attrs-incomplete-5.nix
{ a = { b 1; }; }
```

```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-incomplete-5.nix"
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
      "diagnostics": [],
      "triggerKind": 2
    }
  }
}
```

```
CHECK-DAG: "id": 5,
```

```nix file:///flatten-attrs-incomplete-6.nix
{ a = { b = 1;
```

```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-incomplete-6.nix"
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
      "diagnostics": [],
      "triggerKind": 2
    }
  }
}
```

```
CHECK-DAG: "id": 6,
```

```nix file:///flatten-attrs-incomplete-7.nix
{ a = { b = { c = 1; };
```

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-incomplete-7.nix"
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
      "diagnostics": [],
      "triggerKind": 2
    }
  }
}
```

```
CHECK-DAG: "id": 7,
```

```json
{
  "jsonrpc": "2.0",
  "method": "exit"
}
```
