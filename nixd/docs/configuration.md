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
              // ...
            },
            "options": {
              // ...
            }
        }
  }
}
```

</details>

<details>
 <summary>Neovim</summary>

 Configuration via [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig) plugin. If you want to make configuration changes based on different projects, you can see nvim-lspconfig official [wiki-Project_local_settings](https://github.com/neovim/nvim-lspconfig/wiki/Project-local-settings)

```lua
local nvim_lsp = require("lspconfig")
nvim_lsp.nixd.setup({
   cmd = { "nixd" },
   settings = {
      nixd = {
         nixpkgs = {
            expr = "import <nixpkgs> { }",
         },
         formatting = {
            command = { "nixfmt" },
         },
         options = {
            nixos = {
               expr = '(builtins.getFlake ("git+file://" + toString ./.)).nixosConfigurations.k-on.options',
            },
            home_manager = {
               expr = '(builtins.getFlake ("git+file://" + toString ./.)).homeConfigurations."ruixi@k-on".options',
            },
         },
      },
   },
})
```
</details>

<details>
  <summary>Emacs</summary>

  Configuration via [lsp-mode](https://github.com/emacs-lsp/lsp-mode) plugin. As of Oct 24, 2024, lsp-mode master has support for nixd autocompletion and formatting options.

  ```elisp
(use-package nix-mode
  :after lsp-mode
  :ensure t
  :hook
  (nix-mode . lsp-deferred) ;; So that envrc mode will work
  :custom
  (lsp-disabled-clients '((nix-mode . nix-nil))) ;; Disable nil so that nixd will be used as lsp-server
  :config
  (setq lsp-nix-nixd-server-path "nixd"
        lsp-nix-nixd-formatting-command [ "nixfmt" ]
        lsp-nix-nixd-nixpkgs-expr "import <nixpkgs> { }"
        lsp-nix-nixd-nixos-options-expr "(builtins.getFlake \"/home/nb/nixos\").nixosConfigurations.mnd.options"
        lsp-nix-nixd-home-manager-options-expr "(builtins.getFlake \"/home/nb/nixos\").homeConfigurations.\"nb@mnd\".options"))

(add-hook! 'nix-mode-hook
           ;; enable autocompletion with company
           (setq company-idle-delay 0.1))

  ```

Configuration via [Eglot](https://www.gnu.org/software/emacs/manual/html_node/eglot/).  Eglot has been included in Emacs core since 29.1.  You can configure nixd per project by putting the following snippet into the `.dir-locals.el` file in your project.
```lisp
((nil . ((eglot-workspace-configuration . (:nixd (:options (:nixos (:expr "(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.artemis.options"))))))))
```
You can also set the `eglot-workspace-configuration` variable globally (by using `setq-default`), if you prefer that.

</details>

### Configuration overview

> [!NOTE]
> This annotated json are under the key "nixd". If you don't know what does exactly this mean please see editor examples above.

```jsonc
{
  "nixpkgs": {
    // For flake.
    // "expr": "import (builtins.getFlake \"/home/lyc/workspace/CS/OS/NixOS/flakes\").inputs.nixpkgs { }   "

    // This expression will be interpreted as "nixpkgs" toplevel
    // Nixd provides package, lib completion/information from it.
    ///
    // Resource Usage: Entries are lazily evaluated, entire nixpkgs takes 200~300MB for just "names".
    ///                Package documentation, versions, are evaluated by-need.
    "expr": "import <nixpkgs> { }"
  },
  "formatting": {
    // Which command you would like to do formatting
    "command": [ "nixfmt" ]
  },
  // Tell the language server your desired option set, for completion
  // This is lazily evaluated.
  "options": { // Map of eval information
      // By default, this entriy will be read from `import <nixpkgs> { }`
      // You can write arbitary nix expression here, to produce valid "options" declaration result.
      //
      // *NOTE*: Replace "<name>" below with your actual configuration name.
      // If you're unsure what to use, you can verify with `nix repl` by evaluating
      // the expression directly.
      //
      "nixos": {
          "expr": "(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<name>.options"
      },

    // Before configuring Home Manager options, consider your setup:
    // Which command do you use for home-manager switching?
    //
    //  A. home-manager switch --flake .#... (standalone Home Manager)
    //  B. nixos-rebuild switch --flake .#... (NixOS with integrated Home Manager)
    //
    // Configuration examples for both approaches are shown below.
    "home-manager": {
      // A:
      "expr": "(builtins.getFlake (builtins.toString ./.)).homeConfigurations.<name>.options"

      // B:
      "expr": "(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<name>.options.home-manager.users.type.getSubOptions []"
    }
  },
  // Control the diagnostic system
  "diagnostic": {
    "suppress": [
      "sema-extra-with"
    ]
  }
}
```

### Fields explanation

#### Diagnostic Control ("diagnostic")

Some users might feel confident in their understanding of the language and
prefer to suppress diagnostics altogether. This can be achieved by utilizing the diagnostic field.

```jsonc
{
  "diagnostic": {
    // A list of diagnostic short names
    "suppress": [
      "sema-extra-with"
    ]
  }
}
```

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

    // For flake-parts options.
    // Firstly read the docs here to enable "debugging", exposing declarations for nixd.
    // https://flake.parts/debug
    "flake-parts": {
      "expr": "(builtins.getFlake \"/path/to/your/flake\").debug.options"
    },
    // For a `perSystem` flake-parts option:
    "flake-parts2": {
      "expr": "(builtins.getFlake \"/path/to/your/flake\").currentSystem.options"
    }
  }
}
```

If you aren't a flakes user with standalone home-manager with a vanilla install then the following expression should make home-manager options appear:

```jsonc
{
  "options": {
    "home-manager": {
      "expr": "(import <home-manager/modules> { configuration = ~/.config/home-manager/home.nix; pkgs = import <nixpkgs> {}; }).options"
    }
  }
}
```

## Q & A

### Home-manager options completion does not work. What is `homeConfigurations`?

See [configuration overview](#configuration-overview).
If your home-manager is installed as part of a NixOS module,
the options list can be retrieved like this:

```nix
(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<name>.options.home-manager.users.type.getSubOptions []
```

### Still not working?

Try testing your expression in `nix repl`. This will help you diagnose evaluation issues.
For example:

```
nix-repl> (builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<name>.options
error: syntax error, unexpected SPATH, expecting ID or OR_KW or DOLLAR_CURLY or '"'
  at «string»:1:65:
       1| (builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<name>.options
        |                                                                 ^
```

Replace `<name>` with your actual configuration name:

```
nix-repl> (builtins.getFlake (builtins.toString ./.)).nixosConfigurations.adrastea.options
{
  _module = { ... };
  appstream = { ... };
  assertions = { ... };
  boot = { ... };
  # ... more options ..., this is expected.
}
```
