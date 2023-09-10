## Reproducible Editors Environment

### VSCodium

We provide a test environment with the *vscode-nix-ide* plugin, and the repository has some [configuration examples](/docs/examples) that you can try.

Start up the test environment:

```console
$ nix develop github:nix-community/nixd#vscodium

$ codium-test
```

### Neovim

You can run the following command to edit a *.nix file

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
