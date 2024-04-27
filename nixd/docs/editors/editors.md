## Reproducible Editors Environment

### VSCodium

We provide a test environment with the _vscode-nix-ide_ plugin, and the repository has some [configuration examples](/nixd/docs/examples) that you can try.

Start up the test environment:

```console
$ nix develop github:nix-community/nixd#vscodium

$ codium-test
```

### Neovim

```console
$ nix develop github:nix-community/nixd#nvim

$ nvim-lsp flake.nix
```
