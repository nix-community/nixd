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

```nix file:///flake.nix
{ outputs = { self }: {}; }
```

```
     CHECK: "diagnostics": [],
CHECK-NEXT: "uri": "file:///flake.nix",
```

<-- textDocument/didOpen

```nix file:///flake.nix
{
  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  outputs = { self, nixpkgs }: {};
}
```

```
     CHECK: "diagnostics": [],
CHECK-NEXT: "uri": "file:///flake.nix",
```

<-- textDocument/didOpen

```nix file:///flake.nix
{ outputs = { self, foo }: {}; }
```

```
     CHECK: "diagnostics": [
CHECK-NEXT:   {
CHECK-NEXT:     "code": "sema-unused-def-lambda-noarg-formal",
CHECK-NEXT:     "message": "attribute `foo` of argument is not used",
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 23,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 20,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "relatedInformation": [],
CHECK-NEXT:     "severity": 2,
CHECK-NEXT:     "source": "nixf",
CHECK-NEXT:     "tags": [
CHECK-NEXT:       1
CHECK-NEXT:     ]
CHECK-NEXT:   }
CHECK-NEXT: ],
CHECK-NEXT: "uri": "file:///flake.nix",
```

<-- textDocument/didOpen

```nix file:///foo.nix
{ outputs = { self }: {}; }
```

```
     CHECK: "diagnostics": [
CHECK-NEXT:   {
CHECK-NEXT:     "code": "sema-unused-def-lambda-noarg-formal",
CHECK-NEXT:     "message": "attribute `self` of argument is not used",
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 18,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 14,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "relatedInformation": [],
CHECK-NEXT:     "severity": 2,
CHECK-NEXT:     "source": "nixf",
CHECK-NEXT:     "tags": [
CHECK-NEXT:       1
CHECK-NEXT:     ]
CHECK-NEXT:   }
CHECK-NEXT: ],
CHECK-NEXT: "uri": "file:///foo.nix",
```
