## User Guide

### Installation

Our flake.nix provides a package named `nixd`, and an overlay to nixpkgs that add the `nixd` package.
So, if you would like to use `nixd` in your flake:

```nix
{
  description = "My configuration";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nixd.url = "github:nix-community/nixd";
  };

  outputs = { nixpkgs, nixd, ... }:
    {
      nixosConfigurations = {
        hostname = nixpkgs.lib.nixosSystem
          {
            system = "x86_64-linux";
            modules = [
              {
                nixpkgs.overlays = [ nixd.overlays.default ];
                environment.systemPackages = with pkgs;[
                  nixd
                ];
              }
            ];
          };
      };
    };
}
```

Note that please do NOT override nixpkgs revision for nixd inputs.
The source code have tested on specific version on NixOS/nix, which may not work at your version.

### Build the project from source

This is a guide for build the project from git source.

#### nix-build
``` sh
nix-build --expr 'with import <nixpkgs> { }; callPackage ./. { }'
```

#### Nix Flakes
``` sh
nix build -L .#
```

### Configuration


We support LSP standard `workspace/configuration` for server configurations.

#### Project Installable

Unlike any other nix lsp implementation, you may need to explicitly specify a `installable` in your workspace.
The language server will consider the `installable` is your desired "object file", and it is the cross-file analysis pivot.
For example, here is a nix pacakge:

```nix
{ stdenv
, lib
}:

stdenv.mkDerivation {
    pname = "...";
    version = "...";
}
```

From a language perspective, the lambda expression only accepts one argument and returns an AttrSet, without any special properties of a "package".
So how do we know what argument it will accept?

For nixd, you can write a project-specific *installable* that will be evaluated.
e.g.

```sh
nix eval --expr "with import <nixpkgs> { }; callPackage ./some-package.nix { }"
```

We accept the same argument as `nix eval`, and perform evaluation for language analysis.


```jsonc
"installable": {
    "args":[
        "--expr",
        "with import <nixpkgs> { }; callPackage ./some-package.nix { } "
    ],
    "installable":""
}
```

This is much similar to `compile_commands.json` in C/C++ world.

Here is the demo video that I used the above installable in my workspace:

![package](/docs/images/8d106acc-6b1a-4062-9dc7-175b09751fd0.gif)

#### Evaluation Depth

Nix evaluator will be lazily peform evaluation on your specified task[^nix-evaluation-peformance].

[^nix-evaluation-peformance]: https://nixos.wiki/wiki/Nix_Evaluation_Performance

As for language service, we have an custom extension to nix evaluator that allows you to force thunks being evaluated in a desired depth.

```jsonc
{
    "evalDepth": 5
}
```

#### Workers

Nixd evals your project concurrently.
You can specify how many workers will be used for language tasks, e.g. parsing & evaluation.

```jsonc
{
  "numWorkers": 10
}
```

The default value is `std::thread::hardware_concurrency()`.
