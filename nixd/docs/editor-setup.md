## We ❤️ Contributions!

Please update this doc (tweaks, tricks, or how to setup for another editor)!

## Editor Setup Guide

This is the description about how to setup your editor to use nixd.
Before you do these steps, please ensure that you have a working nixd binary.

Refer to https://github.com/nix-community/nixd/blob/main/nixd/docs/user-guide.md#installation for more information.


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

#### Eglot
The following configuration can be used with the built-in Emacs LSP client: [Eglot](https://www.gnu.org/software/emacs/manual/html_node/eglot/).

```emacs-lisp
(add-hook 'prog-mode-hook
          (lambda ()
            (add-hook 'before-save-hook 'eglot-format nil t)))

(with-eval-after-load 'eglot
  (dolist (mode '((nix-mode . ("nixd"))))
    (add-to-list 'eglot-server-programs mode)))
```

#### lsp-mode

A simple Emacs Lisp configuration that adds nixd to LSP Mode in the mean time is as follows:

```emacs-lisp
(with-eval-after-load 'lsp-mode
  (lsp-register-client
    (make-lsp-client :new-connection (lsp-stdio-connection "nixd")
                     :major-modes '(nix-mode)
                     :priority 0
                     :server-id 'nixd)))
```
