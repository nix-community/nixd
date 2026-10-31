# RUN: nixd --lit-test < %s > %t
# RUN: FileCheck %s --check-prefix=VALUE < %t
# RUN: FileCheck %s --check-prefix=TRIVIA < %t
# RUN: FileCheck %s --check-prefix=SIBLINGS < %t

Single-binding Pack preserves comments and multiline values, including when
bulk actions are also available. Responses may arrive out of order.

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

```nix file:///pack-attrs-comments-2.nix
{
  a.b =
    # IMPORTANT COMMENT
    1;
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-comments-2.nix"
    },
    "range": {
      "start": {
        "line": 1,
        "character": 2
      },
      "end": {
        "line": 1,
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
VALUE: "id": 2,
VALUE: "newText": "a = { b =\n    # IMPORTANT COMMENT\n    1; };",
VALUE-NEXT: "range": {
VALUE-NEXT: "end": {
VALUE-NEXT: "character": 6,
VALUE-NEXT: "line": 3
VALUE-NEXT: },
VALUE-NEXT: "start": {
VALUE-NEXT: "character": 2,
VALUE-NEXT: "line": 1
VALUE: "title": "Pack dotted path to nested set"
```

```nix file:///pack-attrs-comments-3.nix
{
  "a.b" /* BEFORE DOT */ . # AFTER DOT
    "c.d" /* BEFORE EQ */ = # BEFORE VALUE
    ''
      keep this indentation
      # string content
    '' /* BEFORE SEMI */; # AFTER SEMI
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-comments-3.nix"
    },
    "range": {
      "start": {
        "line": 1,
        "character": 2
      },
      "end": {
        "line": 1,
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
TRIVIA: "id": 3,
TRIVIA: "newText": "\"a.b\" /* BEFORE DOT */  = {  # AFTER DOT\n    \"c.d\" /* BEFORE EQ */ = # BEFORE VALUE\n    ''\n      keep this indentation\n      # string content\n    '' /* BEFORE SEMI */; };",
TRIVIA-NEXT: "range": {
TRIVIA-NEXT: "end": {
TRIVIA-NEXT: "character": 25,
TRIVIA-NEXT: "line": 6
TRIVIA-NEXT: },
TRIVIA-NEXT: "start": {
TRIVIA-NEXT: "character": 2,
TRIVIA-NEXT: "line": 1
TRIVIA: "title": "Pack dotted path to nested set"
```

```nix file:///pack-attrs-comments-4.nix
{
  a.b = /* VALUE */ 1 /* SEMI */;
  # SIBLING
  a.c = 2;
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///pack-attrs-comments-4.nix"
    },
    "range": {
      "start": {
        "line": 1,
        "character": 2
      },
      "end": {
        "line": 1,
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
SIBLINGS: "id": 4,
SIBLINGS: "newText": "a = { b = /* VALUE */ 1 /* SEMI */; };",
SIBLINGS-NEXT: "range": {
SIBLINGS-NEXT: "end": {
SIBLINGS-NEXT: "character": 33,
SIBLINGS-NEXT: "line": 1
SIBLINGS-NEXT: },
SIBLINGS-NEXT: "start": {
SIBLINGS-NEXT: "character": 2,
SIBLINGS-NEXT: "line": 1
SIBLINGS: "title": "Pack dotted path to nested set"
```

```json
{
  "jsonrpc": "2.0",
  "method": "exit"
}
```
