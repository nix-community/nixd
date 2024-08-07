<div align="center">
  <h1>nixd</code></h1>

  <p>
    <strong>Nix language server</strong>
  </p>
</div>

## About

This is a feature-rich nix language server interoperating with C++ nix.

Some notable features provided by linking with the Nix library include:

- Nixpkgs option support, for all option system (NixOS/home-manager/flake-parts).
- Nixpkgs package complete, lazily evaluated.
- Shared eval caches (flake, file) with your system's Nix.
- Support for cross-file analysis (goto definition to locations in nixpkgs).


## Get Started

You can *try nixd without installation*.
We have tested some working & reproducible [editor environments](/nixd/docs/editors/editors.md).

## Resources

- [Editor Setup](nixd/docs/editor-setup.md) (get basic working language server out of box)
- [Configuration](nixd/docs/configuration.md) (see how to, and which options are tunable)
- [Features](nixd/docs/features.md) (features explanation)
- [Developers' Manual](nixd/docs/dev.md) (internal design, contributing)
