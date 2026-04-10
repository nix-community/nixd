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
  boot.kernelModules = {
    _type = "option";
    description = "The set of kernel modules to be loaded in the second stage of the boot process.";
    type = {
      description = "list of string";
      name = "listOf";
    };
    default = [ ];
    example = {
      _type = "literalExpression";
      text = "[ \"kvm-intel\" \"snd_hda_intel\" ]";
    };
    declarationPositions = [ ];
  };
  boot.kernelPackages = {
    _type = "option";
    description = "The kernel packages set.";
    type = {
      description = "unspecified value";
      name = "unspecified";
    };
    # `defaultText` must take priority over the raw `default` below:
    # nixpkgs sets `defaultText` precisely because the real default
    # does not render to anything a user could paste back.
    default = "placeholder";
    defaultText = {
      _type = "literalExpression";
      text = "pkgs.linuxPackages";
    };
    declarationPositions = [ ];
  };
  boot.tmp.cleanOnBoot = {
    _type = "option";
    description = "Whether to clean `/tmp` on boot.";
    type = {
      description = "boolean";
      name = "bool";
    };
    default = false;
    declarationPositions = [ ];
  };
}
```

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "{\r\n  boot.bcache.bar = 1;\r\n  boot.binfmt.bar = 1;\r\n  boot.binfmtMiscRegistrations = {\r\n    _type = \"option\";\r\n    description = \"Alias of {option}`boot.binfmt.registrations`.\";\r\n    type = {\r\n      description = \"attribute set of (submodule)\";\r\n      name = \"attrsOf\";\r\n    };\r\n    declarationPositions = [\r\n      { column = 16; file = \"\/nix\/store\/43fgdg04gbrjh8ww8q8zgbqxn4sb35py-source\/lib\/attrsets.nix\"; line = 190; }\r\n    ];\r\n  };\r\n  boot.kernelModules = {\r\n    _type = \"option\";\r\n    description = \"The set of kernel modules to be loaded in the second stage of the boot process.\";\r\n    type = {\r\n      description = \"list of string\";\r\n      name = \"listOf\";\r\n    };\r\n    default = [ ];\r\n    example = {\r\n      _type = \"literalExpression\";\r\n      text = \"[ \\\"kvm-intel\\\" \\\"snd_hda_intel\\\" ]\";\r\n    };\r\n    declarationPositions = [ ];\r\n  };\r\n  boot.kernelPackages = {\r\n    _type = \"option\";\r\n    description = \"The kernel packages set.\";\r\n    type = {\r\n      description = \"unspecified value\";\r\n      name = \"unspecified\";\r\n    };\r\n    default = \"placeholder\";\r\n    defaultText = {\r\n      _type = \"literalExpression\";\r\n      text = \"pkgs.linuxPackages\";\r\n    };\r\n    declarationPositions = [ ];\r\n  };\r\n  boot.tmp.cleanOnBoot = {\r\n    _type = \"option\";\r\n    description = \"Whether to clean `\/tmp` on boot.\";\r\n    type = {\r\n      description = \"boolean\";\r\n      name = \"bool\";\r\n    };\r\n    default = false;\r\n    declarationPositions = [ ];\r\n  };\r\n}"
}
```

An option that has neither `default` nor `example` reports null for both fields.

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/optionInfo",
   "params": [ "boot", "binfmtMiscRegistrations" ]
}
```
```
CHECK:          "Default": null,
CHECK-NEXT:     "Definitions": [],
CHECK-NEXT:     "Description": "Alias of {option}`boot.binfmt.registrations`.",
CHECK-NEXT:     "Example": null,
CHECK-NEXT:     "Type": {
CHECK-NEXT:        "Description": "attribute set of (submodule)",
CHECK-NEXT:        "Name": "attrsOf"
CHECK-NEXT:     }
```

An option with a complex `default` but a `literalExpression` `example` reports
only the example; a raw list default is rejected to avoid deep recursion.

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"attrset/optionInfo",
   "params": [ "boot", "kernelModules" ]
}
```
```
CHECK:          "Default": null,
CHECK-NEXT:     "Definitions": [],
CHECK-NEXT:     "Description": "The set of kernel modules to be loaded in the second stage of the boot process.",
CHECK-NEXT:     "Example": "[ \"kvm-intel\" \"snd_hda_intel\" ]",
CHECK-NEXT:     "Type": {
```

An option with both `default` and `defaultText` uses `defaultText`: the
author set `defaultText` precisely because the raw `default` would be
unhelpful, so it must win (otherwise we would report `"placeholder"`).

```json
{
   "jsonrpc":"2.0",
   "id":3,
   "method":"attrset/optionInfo",
   "params": [ "boot", "kernelPackages" ]
}
```
```
CHECK:          "Default": "pkgs.linuxPackages",
CHECK-NEXT:     "Definitions": [],
CHECK-NEXT:     "Description": "The kernel packages set.",
CHECK-NEXT:     "Example": null,
CHECK-NEXT:     "Type": {
```

An option with a scalar `default` and no `example` falls back to the default.

```json
{
   "jsonrpc":"2.0",
   "id":4,
   "method":"attrset/optionInfo",
   "params": [ "boot", "tmp", "cleanOnBoot" ]
}
```
```
CHECK:          "Default": "false",
CHECK-NEXT:     "Definitions": [],
CHECK-NEXT:     "Description": "Whether to clean `/tmp` on boot.",
CHECK-NEXT:     "Example": null,
CHECK-NEXT:     "Type": {
CHECK-NEXT:        "Description": "boolean",
CHECK-NEXT:        "Name": "bool"
CHECK-NEXT:     }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

