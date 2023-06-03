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
    "capabilities": {
      "workspace": {
        "configuration": true
      }
    },
    "trace": "off"
  }
}
```

<-- initialized

```json
{
  "jsonrpc": "2.0",
  "method": "initialized",
  "params": {}
}
```

Check that we would like to ask for workspace configuration:

--> call workspace/configuration(1)

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "method": "workspace/configuration",
CHECK-NEXT:   "params": {
CHECK-NEXT:     "items": [
CHECK-NEXT:       {
CHECK-NEXT:         "section": "nixd"
CHECK-NEXT:       }
CHECK-NEXT:     ]
CHECK-NEXT:   }
```

<-- reply(1)

This is a valid configuration.
The server should parse it correctly and do not crash.

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "installable": {
        "args": ["--file", "rootInstallable.nix"],
        "installable": ""
      }
    }
  ]
}
```

<-- workspace/didChangeConfiguration

```json
{
  "jsonrpc": "2.0",
  "method": "workspace/didChangeConfiguration",
  "params": {
    "settings": {}
  }
}
```

Check that we will ask for workspace configuration again:

--> call workspace/configuration(2)

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "method": "workspace/configuration",
CHECK-NEXT:   "params": {
CHECK-NEXT:     "items": [
CHECK-NEXT:       {
CHECK-NEXT:         "section": "nixd"
CHECK-NEXT:       }
CHECK-NEXT:     ]
CHECK-NEXT:   }
```

<-- reply(2)

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": [
    {
      "whateverNotExpected": true
    }
  ]
}
```

Do not crash on unexpected configuration!

```json
{ "jsonrpc": "2.0", "method": "exit" }
```
