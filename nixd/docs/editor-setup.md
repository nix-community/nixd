## We ❤ Contributions!

Please update this doc (tweaks, tricks, or how to setup for another editor)!

## Editor Setup Guide

This is the description about how to setup your editor to use nixd.

## Installation - get a working executable

Package `nixd` can be found in [nixpkgs](https://github.com/NixOS/nixpkgs).

<details>
<summary>NixOS Configuration</summary>

```nix
{ pkgs, ... }: {
  environment.systemPackages = with pkgs; [
    nixd
  ];
}
```

</details>

<details>
<summary><b>nix-env</b>(legacy commands)</summary>
On NixOS:

```console
nix-env -iA nixos.nixd
```

On Non NixOS:

```console
nix-env -iA nixpkgs.nixd
```

</details>

<details>
<summary><b>nix profile</b></summary>

```console
nix profile install github:nixos/nixpkgs#nixd
```

</details>

And our flake.nix provides a package named `nixd` with "unstable" experience.

Note that please do NOT override nixpkgs revision for nixd inputs.
The source code have tested on specific version on NixOS/nix, which may not work at your version.

## Teach your editor find the executable, and setup configurations.

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

> [!NOTE]
>
> The client is built-in to lsp-mode after `8.0.0`; you don't need this if you
> have the latest lsp-mode installed.

## Change the configuration.

Read the [configuration](configuration.md) docs here.
