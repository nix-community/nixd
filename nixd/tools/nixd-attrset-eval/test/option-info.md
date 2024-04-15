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
   "method":"attrset/optionInfo",
   "params": [ "boot", "devSize" ]
}
```

```
CHECK: "Description": "Size limit for the /dev tmpfs. Look at mount(8), tmpfs size option,\nfor the accepted syntax.\n",
CHECK: "Example": "32m"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

