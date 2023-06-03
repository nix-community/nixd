<div align="center">
  <h1>nixd</code></h1>

  <p>
    <strong>Nix language server</strong>
  </p>
</div>

## About

This is a Nix language server that directly uses (i.e., is linked with) the official Nix library (https://github.com/NixOS/nix).

Some notable features provided by linking with the Nix library include:

- Diagnostics and evaluation that produce identical results as the real Nix command.
- Shared eval caches (flake, file) with your system's Nix.
- Native support for cross-file analysis.
- Precise Nix language support. We do not maintain "yet another parser & evaluator".
- Support for built-ins, including Nix plugins.

## Features Preview

<details><summary>Handle evaluations exactly same as nix evaluator</summary>

![infinte-recursion](docs/images/9ed5e08a-e439-4b09-ba78-d83dc0a8a03f.png)

</details>

<details><summary>Support *all* builtins</summary>

![eval-builtin-json](docs/images/59655838-36a8-4145-9717-f2009e0efef9.png)

And diagnostic:

![eval-builtin-diagnostic](docs/images/f6e10994-41e4-4a03-84a2-ef275fb402fd.png)

</details>

<details><summary>Eval nixpkgs</summary>

![eval-nixpkgs](docs/images/abe2fafc-d139-4741-89af-53339312a1af.png)

</details>

<details><summary>Print internal AST Node type, and evalution result just as same as nix repl</summary>

![eval-ast](docs/images/c7e8a8c7-5c0e-4736-868f-1e2c345468fd.png)

</details>

<details><summary>Complete dynamic envs, like `with` expression</summary>

![complete-with](docs/images/ae629b9f-95cb-48df-aa1d-4f5f94c3c06a.png)

</details>

## Installation

### Build the project

#### nix-build

```sh
nix-build --expr 'with import <nixpkgs> { }; callPackage ./. { }'
```

#### Nix Flakes

```sh
nix build -L #.
```

#### Development

Here are a short snippet used in our CI, should works fine and reproducible in your development environment.

```
meson setup build/
meson compile -C build
meson test -C build
```

### Editors

- Neovim native LSP and [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig). We are officially supported by nvim-lspconfig, see [upstream docs](https://github.com/neovim/nvim-lspconfig/blob/master/doc/server_configurations.txt#nixd)

## Tooling

We provide some extra tools based on our codebase.

### nix-ast-dump

Used for dumping internal data structures in nix parser.

Demo: [all-grammar.nix](tools/nix-ast-dump/test/all-grammar.nix)

## Resources

- Developers' Manual (internal design, contributing): [Developers' Manual](docs/dev.md)
- Project matrix room: https://matrix.to/#/#nixd:matrix.org
