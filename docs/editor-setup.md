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
We provide a test environment with the *vscode-nix-ide* plugin, and the repository has some [configuration examples](/docs/examples) that you can try.

Start up the test environment:
```console
$ nix develop github:nix-community/nixd#vscodium

$ codium-test
```

### (Neo)vim

#### Coc.nvim

According to `:help coc-config.txt`, `coc-settings.json`:

```jsonc
{
  "languageserver": {
    "nix": {
      "command": "nixd",
      "filetypes": ["nix"]
    }
  }
}
```

### Neovim

Neovim native LSP and [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig).
We are officially supported by nvim-lspconfig, see [upstream docs](https://github.com/neovim/nvim-lspconfig/blob/master/doc/server_configurations.txt#nixd)

Also, we provide an [example config](/editors/nvim-lsp.nix) for testing, You can run the following command to edit a *.nix file
```console
$ nix develop github:nix-community/nixd#nvim

$ nvim-lsp /tmp/test/default.nix
```
tip: If you want to configure lsp itself, see [configuration](/docs/user-guide.md#configuration), and the following tree-like directory
```console
# tree -a /tmp/test
/tmp/test/
├── default.nix
└── .nixd.json
```
