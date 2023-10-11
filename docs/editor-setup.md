## We ❤️ Contributions!

Please update this doc (tweaks, tricks, or how to setup for another editor)!

## Editor Setup Guide

This is the description about how to setup your editor to use nixd.
Before you do these steps, please ensure that you have a working nixd binary.

Refer to https://github.com/nix-community/nixd/blob/main/docs/user-guide.md#installation for more information.


### VSCode

https://github.com/nix-community/vscode-nix-ide extension provide a general interface for nixd, and it should work out of box.
Please file a bug if you encountered some trouble using the extension.

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

### Emacs

[This PR](https://github.com/emacs-lsp/lsp-mode/pull/4125) should add nixd support to [LSP Mode](https://github.com/emacs-lsp/lsp-mode).

A simple Emacs Lisp configuration that adds nixd to LSP Mode in the mean time is as follows:

```lisp
(with-eval-after-load 'lsp-mode
  (lsp-register-client
    (make-lsp-client :new-connection (lsp-stdio-connection "nixd")
                     :major-modes '(nix-mode)
                     :priority 0
                     :server-id 'nixd)))
```