## User Guide

### Installation
At this time (2023-06-14), nixd is under rapid development and it is highly recommended to install nixd from source.

Package `nixd` can be found in [nixpkgs](https://github.com/NixOS/nixpkgs), there are different ways to install nixd, pick your favourite:

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

And our flake.nix provides a package named `nixd`, and an overlay to nixpkgs that add the `nixd` package.

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


- [Configuration Examples](/nixd/docs/examples)

We support LSP standard `workspace/configuration` for server configurations.

Configuration overview:

```jsonc
{
  // The evaluation section, provide auto completion for dynamic bindings.
  "eval": {
    "target": {
      // Accept args as "nix eval"
      "args": [],
      // "nix eval"
      "installable": ""
    },
    // Extra depth for evaluation
    "depth": 0,
    // The number of workers for evaluation task.
    "workers": 3
  },
  "formatting": {
    // Which command you would like to do formatting
    "command": "nixpkgs-fmt"
  },
  // Tell the language server your desired option set, for completion
  // This is lazily evaluated.
  "options": {
    // Enable option completion task.
    // If you are writing a package, disable this
    "enable": true,
    "target": {
      // Accept args as "nix eval"
      "args": [],
      // "nix eval"
      "installable": ""
    }
  }
}
```

Note: we support a configuration file named `.nixd.json` at your workspace directory.
This is a feature requested by nvim users.

Typically, you can write a nix file, and evaluate the result into `.nixd.json`, because json does not support comments:

```nix
# .nixd.nix
{
  eval = {
    # Example target for writing a package.
    target = {
      args = [ "--expr" "with import <nixpkgs> { }; callPackage ./somePackage.nix { }" ];
      installable = "";
    };
    # Force thunks
    depth = 10;
  };
  formatting.command = "nixpkgs-fmt";
  options = {
    enable = true;
    target = {
      args = [ ];
      # Example installable for flake-parts, nixos, and home-manager

      # flake-parts
      installable = "/flakeref#debug.options";

      # nixOS configuration
      installable = "/flakeref#nixosConfigurations.<adrastea>.options";

      # home-manager configuration
      installable = "/flakeref#homeConfigurations.<name>.options";
    };
  };
}

```

```console
nix eval --json --file .nixd.nix > .nixd.json
```

`.nixd.json`, the configuration of `nixd`, supports [json schema](https://json-schema.org/).
So if your editor supports
[LSP](https://microsoft.github.io/language-server-protocol/implementors/servers/),
you can get completions, diagnostics and more[^json-schema]:

![completion](https://github.com/nix-community/nixd/assets/32936898/013367c6-63d8-4bba-9057-c5701c2f36c1)

![diagnostic](https://github.com/nix-community/nixd/assets/32936898/d285eb69-f023-4573-a8e5-8d5501ae3d16)

[^json-schema]: These pictures were captured in [neovim](https://neovim.org/)
with the plugin [coc-json](https://github.com/neoclide/coc-json).

#### Evaluation

##### Target

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
{
  "eval": {
    "target": {
      // Same as:
      // nix eval --expr "..."
      "args": [
        "--expr",
        "with import <nixpkgs> { }; callPackage ./some-package.nix { } "
      ],
      // AttrPath
      "installable": ""
    }
  }
}
```

This is much similar to `compile_commands.json` in C/C++ world.

Here is the demo video that I used the above installable in my workspace:

![write-package](https://github.com/nix-community/nixd/assets/36667224/a974c60e-096e-4964-a5d4-fc926963d577)

##### Depth

Nix evaluator will be lazily peform evaluation on your specified task[^nix-evaluation-peformance].

[^nix-evaluation-peformance]: https://wiki.nixos.org/wiki/Nix_Evaluation_Performance

As for language service, we have an custom extension to nix evaluator that allows you to force thunks being evaluated in a desired depth.

```jsonc
{
    "eval": {
      "depth": 5
    }
}
```

##### Workers

Nixd evals your project concurrently.
You can specify how many workers will be used for language tasks, e.g. parsing & evaluation.

```jsonc
{
    "eval": {
      "workers": 5
    }
}
```

The default value is `std::thread::hardware_concurrency()`.

#### Format

To configure which command will be used for formatting, you can change the "formatting" section.

```jsonc
{
  "formatting": {
    // The external command to be invoked for formatting
    "command": ""
  }
}
```

#### Options

This is our support for nixpkgs option system.

Generally options are merged under a special attribute path.
For example, NixOS options could be found at:

```
<flakeref>#nixosConfigurations.<name>.options
```

And, home-manager options also could be found at:

```
<flakeref>#homeConfigurations.<name>.options
```

In our option system, you need to specify which option set you'd like to use.

```jsonc
{
  "options": {
    // Disable it if you are not writting modules.
    "enable": true,
    "target": {
      "args": [],
      // Example of NixOS options.
      "installable": "<flakeref>#nixosConfigurations.<name>.options"
    }
  }
}
```

<details><summary>Options auto completion</summary>

**Home-manager Options**

![hm-docs](https://github.com/nix-community/nixd/assets/36667224/38039c75-379f-463f-aac8-e33ff71eea38)

**NixOS Options**

![nixos-option-docs](https://github.com/nix-community/nixd/assets/36667224/ca4ed4dc-469f-4c2b-9dea-7beab9a417e8)


</details>


### FAQ

#### How to use nixd in my *flake*?

The *eval* subsystems requires flakes evaluating **in-place** to get language callbacks.
The *options* subsystem does not need to eval flakes in-place.

However, nix flakes are now hardcoded being evaluated in your store, e.g. `/nix/store`.
That is, we cannot hack caches by injecting our own data structre.
So basically language callbacks (i.e. dynamic bindings & values) are not available.

Actually we are waiting for [Source tree abstraction (by edolstra)](https://github.com/NixOS/nix/pull/6530), to handle this issue.

If you would like to use `nixd` in your personal flake, you can use `flake-compat` to turn your project in a "non-flake" installable.

Note that `flake-compat` by edolstra will fetch a git project in nix store, that will break everything just as the same case as normal flakes (i.e. not being evaluated in-pllace).
Here we have a fork of `flake-compat`, won't fetch git repositories at `github:inclyc/flake-compat`.

So tldr, to use `nixd` in your flake project, you have to:

1. Turn your project into a legacy one, by using `flake-compat`
2. Use `inclyc/flake-compat` which will not fetch git repository in nix store

We have a working example [here](/nixd/docs/examples/flake/)
