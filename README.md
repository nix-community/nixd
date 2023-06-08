<div align="center">
  <h1>nixd</code></h1>

  <p>
    <strong>Nix language server</strong>
  </p>
</div>

## About

This is a Nix language server that directly uses (i.e., is linked with) the official Nix library (https://github.com/NixOS/nix).

Some notable features provided by linking with the Nix library include:

- Nixpkgs option support, for all option system (NixOS/home-manager/flake-parts).
- Diagnostics and evaluation that produce identical results as the real Nix command.
- Shared eval caches (flake, file) with your system's Nix.
- Native support for cross-file analysis.
- Precise Nix language support. We do not maintain "yet another parser & evaluator".
- Support for built-ins, including Nix plugins.


## Features Preview


<details><summary>Home-manager options auto-complete & goto declaration</summary>

![hm-options-decl](https://user-images.githubusercontent.com/36667224/244408335-5c2b40a9-48da-4214-9071-5f80fcb721ae.gif)

See how to configure option system: https://github.com/nix-community/nixd/blob/main/docs/user-guide.md#options

</details>

<details><summary>Write a package using nixd</summary>

![package](/docs/images/8d106acc-6b1a-4062-9dc7-175b09751fd0.gif)

</details>

<details><summary>Native cross-file analysis</summary>

![package](docs/images/3e4fc99c-7a20-42be-a337-d1746239c731.png)

See how to configure the evaluator for cross-file analysis: https://github.com/nix-community/nixd/blob/main/docs/user-guide.md#evaluation

</details>

<details><summary>Handle evaluations exactly same as nix evaluator</summary>

![infinte-recursion](docs/images/9ed5e08a-e439-4b09-ba78-d83dc0a8a03f.png)

</details>

<details><summary>Support *all* builtins</summary>

![eval-builtin-json](docs/images/59655838-36a8-4145-9717-f2009e0efef9.png)

And diagnostic:

![eval-builtin-diagnostic](docs/images/f6e10994-41e4-4a03-84a2-ef275fb402fd.png)

</details>

## Resources

- [User Guide](docs/user-guide.md)
- [Developers' Manual](docs/dev.md) (internal design, contributing):
- Project matrix room: https://matrix.to/#/#nixd:matrix.org

## Tooling

We provide some extra tools based on our codebase.

### nix-ast-dump

Used for dumping internal data structures in nix parser.

Demo: [all-grammar.nix](tools/nix-ast-dump/test/all-grammar.nix)

