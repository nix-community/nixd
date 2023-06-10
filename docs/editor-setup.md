## We ❤️ Contributions!

Please update this doc (tweaks, tricks, or how to setup for another editor)!

## Editor Setup Guide

This is the description about how to setup your editor to use nixd.
Before you do these steps, please ensure that you have a working nixd binary.

Refer to https://github.com/nix-community/nixd/blob/main/docs/user-guide.md#installation for more information.


### VSCode

https://github.com/nix-community/vscode-nix-ide extension provide a general interface for nixd, and it should work out of box.
Please file a bug if you encountered some trouble using the extension.




```jsonc
{
    "nix.enableLanguageServer": true,
    // Use absolute path if it is not in `PATH`.
    "nix.serverPath": "nixd",
    "nix.serverSettings": {
        "nixd": {
            "eval": {
                // stuff
            },
            "formatting": {
                "command": "nixpkgs-fmt"
            },
            "options": {
                "enable": true,
                "target": {
                    // tweak arguments here
                    "args": [],
                    // NixOS options
                    "installable": "<flakeref>#nixosConfigurations.<name>.options"

                    // Flake-parts options
                    // "installable": "<flakeref>#debug.options"

                    // Home-manager options
                    // "installable": "<flakeref>#homeConfigurations.<name>.options"
                }
            }
        }
   }
}
```
