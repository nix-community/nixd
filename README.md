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
