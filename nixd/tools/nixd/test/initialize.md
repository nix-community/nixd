# RUN: nixd --lit-test < %s | FileCheck %s

Check basic handshake with the server, i.e. "initialize"

<-- initialize(0)

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"initialize",
   "params":{
      "processId":123,
      "rootPath":"",
      "capabilities":{ },
      "trace":"off"
   }
}
```

<-- reply:initialize(0)

```
     CHECK: {
CHECK-NEXT:   "id": 0,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "capabilities": {
CHECK-NEXT:       "codeActionProvider": {
CHECK-NEXT:         "codeActionKinds": [
CHECK-NEXT:           "quickfix"
CHECK-NEXT:         ],
CHECK-NEXT:         "resolveProvider": false
CHECK-NEXT:       },
CHECK-NEXT:       "definitionProvider": true,
CHECK-NEXT:       "documentHighlightProvider": true,
CHECK-NEXT:       "documentSymbolProvider": true,
CHECK-NEXT:       "hoverProvider": true,
CHECK-NEXT:       "referencesProvider": true,
CHECK-NEXT:       "renameProvider": {
CHECK-NEXT:         "prepareProvider": true
CHECK-NEXT:       },
CHECK-NEXT:       "semanticTokensProvider": {
CHECK-NEXT:         "full": true,
CHECK-NEXT:         "legend": {
CHECK-NEXT:           "tokenModifiers": [
CHECK-NEXT:             "static",
CHECK-NEXT:             "abstract",
CHECK-NEXT:             "async",
CHECK-NEXT:             "readonly",
CHECK-NEXT:             "deprecated",
CHECK-NEXT:             "declaration"
CHECK-NEXT:           ],
CHECK-NEXT:           "tokenTypes": [
CHECK-NEXT:             "function",
CHECK-NEXT:             "string",
CHECK-NEXT:             "number",
CHECK-NEXT:             "type",
CHECK-NEXT:             "keyword",
CHECK-NEXT:             "variable",
CHECK-NEXT:             "interface",
CHECK-NEXT:             "variable",
CHECK-NEXT:             "regexp",
CHECK-NEXT:             "macro",
CHECK-NEXT:             "method",
CHECK-NEXT:             "regexp",
CHECK-NEXT:             "regexp"
CHECK-NEXT:           ]
CHECK-NEXT:         },
CHECK-NEXT:         "range": false
CHECK-NEXT:       },
CHECK-NEXT:       "textDocumentSync": {
CHECK-NEXT:         "change": 2,
CHECK-NEXT:         "openClose": true,
CHECK-NEXT:         "save": true
CHECK-NEXT:       }
CHECK-NEXT:     }
CHECK-NEXT:     "serverInfo": {
CHECK-NEXT:       "name": "nixd",
CHECK-NEXT:       "version": {{.*}}
CHECK-NEXT:     }
CHECK-NEXT:   }
CHECK-NEXT: }
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


```json
{"jsonrpc":"2.0","method":"exit"}
```
