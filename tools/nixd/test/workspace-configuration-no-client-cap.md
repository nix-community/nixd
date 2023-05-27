# RUN: nixd --lit-test < %s | FileCheck %s


--> initialize(0)

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"initialize",
   "params":{
      "processId":123,
      "rootPath":"",
      "capabilities":{
        "workspace": {
            "configuration": false
        }
      },
      "trace":"off"
   }
}
```

<-- initialized

```json
{
   "jsonrpc":"2.0",
   "method":"initialized",
   "params":{

   }
}
```

Check that we won't ask for workspace configuration, if the client does not support.

```
CHECK-NOT: "method": "workspace/configuration",
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
