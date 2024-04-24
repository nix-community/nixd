## Configuration


We support LSP standard `workspace/configuration` for server configurations.

### Default configuration & Who needs configuration

Most important part of package/options features is the path of "nixpkgs".
By default, this is search via standard nix search path.
That is, find nixpkgs via `<nixpkgs>`.

For nix-channels users: nixos option & package features shall work out of box, without any extra effort.

For nix-flake users: (suggestion) you can set $NIX_PATH env to your flake input. e.g.

```nix
{ inputs, ... }:
{
  # NixOS configuration.
  nix.nixPath = [ "nixpkgs=${inputs.nixpkgs}" ];
}
```

If you do not config anything in nixd, here are the defaults (suitable for most nix newbies).

* nixpkgs packages comes from `import <nixpkgs> { }`
* nixos options, evaluated from `<nixpkgs>`

For advanced users, e.g. having many custom modules, or want to extend anyother options system,
"options" field must be extended in the settings, to tell nixd how to get the modules. tldr:

* custom module system (home-manager, nix-darwin, flake-parts, ...)
* custom nixpkgs path, input from your "system" flake


### Where to place the configuration

> In legacy versions (v1.x), configurations are written in ".nixd.json", please remove them and nixd won't even read such files anymore.

All configuration should go in `nixd` section.
`nixd` accept language server protocol specified `workspace/configuration` to fetch config file.
So the location of configuration file basically determined by your editor setup.


> Please extend this list for your editor, thanks!

<details>
 <summary>VSCode</summary>

For vscode users you should write `settings.json`[^settings] like this:

[^settings]: the file could be applied per-workspace, i.e. `.vscode/settings.json`, our globally, remotely. VSCode users should familiar with [these locations](https://code.visualstudio.com/docs/getstarted/settings).


```
{
    "nix.serverSettings": {
        // settings for 'nixd' LSP
        "nixd": {
            "formatting": {
                // This is the default if ommited.
                "command": [ "nixpkgs-fmt" ]
            },
            "options": {
                // By default, this entriy will be read from `import <nixpkgs> { }`
                // You can write arbitary nix expression here, to produce valid "options" declaration result.
                // Tip: for flake-based configuration, utilize `builtins.getFlake`
                "nixos": {
                    "expr": "(builtins.getFlake \"/absolute/path/to/flake\").nixosConfigurations.<name>.options"
                },
                "home-manager": {
                    "expr": "(builtins.getFlake \"/absolute/path/to/flake\").homeConfigurations.<name>.options"
                }
            }
        }
  }
}
```

</details>

<details>
 <summary>Neovim</summary>
 Write your settings in Lua, in `setup()` function argument.

```lua
    nvim_lsp.nixd.setup({
        on_attach = on_attach(),
        capabilities = capabilities,
        settings = {
            nixd = {
                nixpkgs = {
                    expr = "import <nixpkgs> { }",
                },
                formatting = {
                    command = { "nixpkgs-fmt" },
                },
                options = {
                    nixos = {
                        expr = '(builtins.getFlake "/tmp/NixOS_Home-Manager").nixosConfigurations.hostname.options',
                    },
                    home_manager = {
                        expr = '(builtins.getFlake "/tmp/NixOS_Home-Manager").homeConfigurations."user@hostname".options',
                    },
                },
            },
        },
    })
```
</details>

### Configuration overview

> Note: This annotated json are under the key "nixd". If you don't know what does exactly this mean please see editor examples above.

```jsonc
{
  "nixpkgs": {
    // For flake.
    // "expr": "import (builtins.getFlake \"/home/lyc/workspace/CS/OS/NixOS/flakes\").inputs.nixpkgs { }   "

    // This expression will be interpreted as "nixpkgs" toplevel
    // Nixd provides package, lib completion/information from it.
    ///
    // Resouces Usage: Entries are lazily evaluated, entire nixpkgs takes 200~300MB for just "names".
    ///                Package documentation, versions, are evaluated by-need.
    "expr": "import <nixpkgs> { }"
  },
  "formatting": {
    // Which command you would like to do formatting
    "command": [ "nixpkgs-fmt" ]
  },
  // Tell the language server your desired option set, for completion
  // This is lazily evaluated.
  "options": { // Map of eval information
    // If this is ommited, default search path (<nixpkgs>) will be used.
    "nixos": { // This name "nixos" could be arbitary.
      // The expression to eval, intepret it as option declarations.
      "expr": "(builtins.getFlake \"/home/lyc/flakes\").nixosConfigurations.adrastea.options"
    },

    // By default there is no home-manager options completion, thus you can add this entry.
    "home-manager": {
      "expr": "(builtins.getFlake \"/home/lyc/flakes\").homeConfigurations.\"lyc@adrastea\".options"
    }
  }
}
```

### Fields explanation

#### Format ("formating")

To configure which command will be used for formatting, you can change the "formatting" section.

```jsonc
{
  "formatting": {
    // The external command to be invoked for formatting
    "command": [ "some-command" ]
  }
}
```

#### Options ("options")

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
  // Tell the language server your desired option set, for completion
  // This is lazily evaluated.
  "options": { // Map of eval information
    // If this is ommited, default search path (<nixpkgs>) will be used.
    "nixos": { // This name "nixos" could be arbitary.
      // The expression to eval, intepret it as option declarations.
      "expr": "(builtins.getFlake \"/home/lyc/flakes\").nixosConfigurations.adrastea.options"
    },

    // By default there is no home-manager options completion, thus you can add this entry.
    "home-manager": {
      "expr": "(builtins.getFlake \"/home/lyc/flakes\").homeConfigurations.\"lyc@adrastea\".options"
    },

    // For flake-parts opitons.
    // Firstly read the docs here to enable "debugging", exposing declarations for nixd.
    // https://flake.parts/debug
    "flake-parts": {
      "expr": "(builtins.getFlake \"/path/to/your/flake\").debug.options"
    }
  }
}
```


