# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```nix
{
  boot.bcache.bar = 1;
  boot.binfmt.bar = 1;
  boot.binfmtMiscRegistrations = {
    _type = "option";
    description = "Alias of {option}`boot.binfmt.registrations`.";
    type = {
      description = "attribute set of (submodule)";
      name = "attrsOf";
    };
    declarationPositions = [
      { column = 16; file = "/nix/store/43fgdg04gbrjh8ww8q8zgbqxn4sb35py-source/lib/attrsets.nix"; line = 190; }
    ];
  };
}
```

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "{\r\n  boot.bcache.bar = 1;\r\n  boot.binfmt.bar = 1;\r\n  boot.binfmtMiscRegistrations = {\r\n    _type = \"option\";\r\n    description = \"Alias of {option}`boot.binfmt.registrations`.\";\r\n    type = {\r\n      description = \"attribute set of (submodule)\";\r\n      name = \"attrsOf\";\r\n    };\r\n    declarationPositions = [\r\n      { column = 16; file = \"\/nix\/store\/43fgdg04gbrjh8ww8q8zgbqxn4sb35py-source\/lib\/attrsets.nix\"; line = 190; }\r\n    ];\r\n  };\r\n}"
}
```

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/optionInfo",
   "params": [ "boot", "binfmtMiscRegistrations" ]
}
```
```
CHECK:          "Description": "Alias of {option}`boot.binfmt.registrations`.",
CHECK-NEXT:     "Example": null,
CHECK-NEXT:     "Type": {
CHECK-NEXT:        "Description": "attribute set of (submodule)",
CHECK-NEXT:        "Name": "attrsOf"
CHECK-NEXT:     }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

