# RUN: nixd --lit-test \
# RUN: --nixos-options-expr='let roleType = { name = "enum"; description = "one of \"server\", \"agent\""; functor.payload.values = [ "server" "agent" ]; }; in { demo.role = { _type = "option"; type = roleType; }; }' \
# RUN: < %s | FileCheck %s

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

```nix file:///basic.nix
{ demo.role = "wrongValue"; }
```

```
     CHECK: "method": "textDocument/publishDiagnostics",
     CHECK: "code": "option-value-type",
     CHECK: "message": "definition `\"wrongValue\"` for option `demo.role` is not of type `one of \"server\", \"agent\"`.",
     CHECK: "severity": 1,
     CHECK: "source": "nixd"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
