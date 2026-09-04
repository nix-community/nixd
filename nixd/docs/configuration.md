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

Configuration via Neovim [built-in LSP](https://neovim.io/doc/user/lsp/#lsp). If you want to make configuration changes based on different projects, you can use Neovim official [exrc](https://neovim.io/doc/user/options/#'exrc') option.

```lua
local nvim_lsp = vim.lsp
nvim_lsp.config("nixd", {
  cmd = { "nixd" },
  filetypes = { "nix" },
  root_markers = { "flake.nix", ".git" },
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
          expr = '(builtins.getFlake (toString ./.)).nixosConfigurations.<hostname>.options',
        },
        home_manager = {
          expr = '(builtins.getFlake (toString ./.)).homeConfigurations."<username>@<hostname>".options',
        },
      },
    },
  },
})
nvim_lsp.enable("nixd")
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
  },
  // Honour a `#:schema <module>` line at the top of a document. Off by default.
  "schemaDirective": {
    "enable": false
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

#### Schema directive ("schemaDirective")

The `options` above are a property of the *workspace*: every configured option
set is offered in every attrset of every file. A document can instead name the
module that declares its own options, in its leading comment block:

```nix
#:schema ./my-app.module.nix
{
  outputs = {
    buffer = 16;
  };
}
```

The path is resolved relative to the document's own directory, and evaluated as

```
let pkgs = <nixpkgs.expr>; in
(pkgs.lib.evalModules {
  modules = [ <path> { _module.args.pkgs = pkgs.lib.mkDefault pkgs; } ];
}).options
```

so a module may take `{ lib, pkgs, ... }:` as it would under NixOS. It is
re-evaluated when its file changes on disk, which matters for generated modules
that an application rewrites underneath an open config.

For a document that carries the directive, that module *replaces* the
workspace-configured option sets rather than adding to them — a config that
says which options it accepts does not also want every unrelated NixOS option.
Documents without a directive are unaffected.

The spelling is [taplo]'s, so a generator that emits a JSON Schema for a TOML
config and a module for a Nix config can write the same line into both.

The feature is off by default, because it makes nixd evaluate Nix code named by
a file you merely opened, rather than by your editor settings: opening a
repository you do not trust could evaluate code from it. Enable it with

```jsonc
{ "schemaDirective": { "enable": true } }
```

Even then, the path must resolve inside the document's own directory or the
workspace root, and be spellable as a Nix path literal; anything else is refused
and logged.

[taplo]: https://taplo.tamasfe.dev/configuration/directives.html

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
