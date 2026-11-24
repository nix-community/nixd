# RUN: nixd --lit-test < %s | FileCheck %s

With one name too: the binding that replaces the inherit moves `__curPos`: no action.

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

```nix file:///curpos-from-single.nix
{ inherit (__curPos) line; }
```

<-- textDocument/codeAction(2) at 0:21

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///curpos-from-single.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 21
         },
         "end": {
            "line": 0,
            "character": 21
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
 CHECK-NOT: "title": "Convert to explicit binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
