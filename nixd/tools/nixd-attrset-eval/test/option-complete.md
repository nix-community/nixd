# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "(let pkgs = import <nixpkgs> { }; in (pkgs.lib.evalModules { modules =  (import <nixpkgs/nixos/modules/module-list.nix>) ++ [ ({...}: { nixpkgs.hostPlatform = builtins.currentSystem;} ) ] ; })).options"
}
```


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/optionComplete",
   "params": { "Scope": [ "boot" ], "Prefix": "" }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
CHECK-NEXT:   {
CHECK-NEXT:     "Description": null,
CHECK-NEXT:     "Name": "bcache"
CHECK-NEXT:   },
CHECK-NEXT:   {
CHECK-NEXT:     "Description": null,
CHECK-NEXT:     "Name": "binfmt"
CHECK-NEXT:   },
CHECK-NEXT:   {
CHECK-NEXT:     "Description": {
CHECK-NEXT:       "Declarations": [
CHECK-NEXT:         {
CHECK-NEXT:           "range": {
CHECK-NEXT:             "end": {
     CHECK:     "Description": "Alias of {option}`boot.binfmt.registrations`.",
CHECK-NEXT:     "Example": null,
CHECK-NEXT:     "Type": {
CHECK-NEXT:        "Description": "attribute set of (submodule)",
CHECK-NEXT:        "Name": "attrsOf"
CHECK-NEXT:     }
CHECK-NEXT:   },
CHECK-NEXT:   "Name": "binfmtMiscRegistrations"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

