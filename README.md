<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Overpass:wght@300&display=swap" rel="stylesheet">

<div align="center" style="font-family: 'Overpass', sans-serif;">
  <h1 style="color: #6586c8;">nixd</code></h1>

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
- Native support for cross-file analysis (goto definition to locations in nixpkgs).
- Precise Nix language support. We do not maintain "yet another parser & evaluator".
- Support for built-ins, including Nix plugins.


## Features Preview


<details><summary>Home-manager options auto-complete & goto declaration</summary>

![options-example](https://github.com/nix-community/nixd/assets/36667224/43e00a8e-c2e6-4598-b188-f5e95d708256)

See how to configure option system: https://github.com/nix-community/nixd/blob/main/docs/user-guide.md#options

</details>

<details><summary>Write a package using nixd</summary>

![write-package](https://github.com/nix-community/nixd/assets/36667224/a974c60e-096e-4964-a5d4-fc926963d577)

</details>

<details><summary>Native cross-file analysis</summary>

![package](docs/images/3e4fc99c-7a20-42be-a337-d1746239c731.png)

We support goto-definition on nix derivations!
Just `Ctrl + click` to see where is a package defined.

![goto-def-pkg-2](https://github.com/nix-community/nixd/assets/36667224/726c711f-cf75-48f4-9f3b-40dd1b9f53be)

And also for nix lambda:

![lambda-location](https://github.com/nix-community/nixd/assets/36667224/5792da0b-8152-4e51-9b0e-0387b045eeb5)

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

- [Editor Setup](docs/editor-setup.md)
- [User Guide](docs/user-guide.md)
- [Configuration Examples](docs/examples)
- [Developers' Manual](docs/dev.md) (internal design, contributing):
- Project matrix room: https://matrix.to/#/#nixd:matrix.org

## Tooling

We provide some extra tools based on our codebase.

### nix-ast-dump

Used for dumping internal data structures in nix parser.

Demo: [all-grammar.nix](tools/nix-ast-dump/test/all-grammar.nix)

