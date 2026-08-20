# Configuration

nixd combines its configuration sources through one strict patch format. This
page describes that format, startup project loading, and live editor overlays.

## Defaults

Without configuration, nixd uses the standard Nix search path:

- packages come from `import <nixpkgs> { }`;
- NixOS options come from the modules under `<nixpkgs>`; and
- formatting uses `nixfmt`.

This works directly for many channel-based installations. Flake-based setups
usually configure `nixpkgs` and `options` expressions explicitly, or arrange
for their flake input to appear in `NIX_PATH`.

## Sources and precedence

Configuration is built in this order:

1. Start with the built-in defaults.
2. Apply the explicitly supplied legacy `--nixpkgs-expr` and
   `--nixos-options-expr` flags.
3. Apply either the JSON passed to `--config` or an opted-in project
   `.nixd.json` file. `--config` takes precedence and suppresses all project
   inspection.
4. At runtime, overlay the latest valid editor patch on the resulting immutable
   startup base.

Each source uses the same [patch format](#patch-format). JSON passed to
`--config` and JSON in `.nixd.json` are unwrapped patch objects. Editor settings
provide that object under the editor's `nixd` section because nixd requests the
`nixd` section through LSP `workspace/configuration`.

For example, this is an unwrapped patch for `--config` or `.nixd.json`:

```json
{
  "formatting": { "command": ["nixfmt"] },
  "nixpkgs": {
    "expr": "import (builtins.getFlake (builtins.toString ./.)).inputs.nixpkgs { }"
  }
}
```

The equivalent editor settings have a `nixd` wrapper:

```json
{
  "nixd": {
    "formatting": { "command": ["nixfmt"] },
    "nixpkgs": {
      "expr": "import (builtins.getFlake (builtins.toString ./.)).inputs.nixpkgs { }"
    }
  }
}
```

Invalid JSON or an invalid patch passed through `--config` is fatal during
startup.

## Project configuration

> [!WARNING]
> Enable project configuration only for workspaces you trust. A project patch
> can provide Nix expressions for nixd to evaluate and formatter programs for
> nixd to execute.

Project configuration is disabled by default. Opt in with:

```console
nixd --enable-project-config
```

The opt-in is startup-only. nixd selects one root while processing the LSP
`initialize` request and reads exactly `<root>/.nixd.json`. It does not search
ancestors, watch the file, reload it, discover nested project files, or
implement general multi-root project configuration. Restart nixd after changing
the file.

If `--config` is present, nixd does not inspect project roots or `.nixd.json`,
even when `--enable-project-config` is also present.

### Root selection

Before selecting a root, nixd refuses project loading if the raw
`workspaceFolders` array contains more than one entry. Otherwise it considers
exactly one candidate in this order:

1. `rootUri`, if supplied;
2. a non-empty `rootPath`, if supplied;
3. the sole workspace folder, if supplied; or
4. the directory from which nixd was launched.

An invalid or non-file URI, or a missing, non-directory, or otherwise unusable
path selected as the higher-priority candidate, does not fall through to a
lower-priority candidate.
nixd reports one startup warning and keeps the built-ins plus explicit legacy
flags, using the launch directory as its evaluator and formatter working
directory.

A missing `<root>/.nixd.json` is silent and also keeps that startup base and the
launch working directory. An unreadable file, invalid JSON, or schema-invalid
patch reports one startup warning and uses the same fallback. A valid project
file, including `{}`, applies its patch and rebases evaluator and formatter
execution to the selected root. No other startup or editor path changes the
launch working directory.

### Example `.nixd.json`

```json
{
  "$schema": "https://json.schemastore.org/nixd-schema.json",
  "nixpkgs": {
    "expr": "import (builtins.getFlake (builtins.toString ./.)).inputs.nixpkgs { }"
  },
  "options": {
    "nixos": {
      "expr": "(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<hostname>.options"
    },
    "home-manager": {
      "expr": "(builtins.getFlake (builtins.toString ./.)).homeConfigurations.\"<user>@<hostname>\".options"
    }
  },
  "formatting": {
    "command": ["nixfmt"]
  },
  "diagnostic": {
    "suppress": ["sema-extra-with"]
  }
}
```

Replace the angle-bracket placeholders with actual flake output names.

## Editor configuration

nixd requests one `workspace/configuration` item with section `nixd`. A valid
response is an array containing exactly one item: either a patch object or
`null`, which resets the active configuration to the startup base. Each
accepted editor patch is applied to the immutable startup base, not to the
previous editor patch, so omitted fields restore that field from the startup
base.

The responses `[null]` and `[{}]` both reset the active configuration to the
startup base. An empty response array, arrays of the wrong length, non-array
responses, malformed patches, transport errors, and responses made stale by a
newer request do not alter the active configuration. Invalid current responses
are logged; stale responses are ignored.

### Neovim

With Neovim's built-in LSP client, put the patch under `settings.nixd`:

```lua
vim.lsp.config("nixd", {
  cmd = { "nixd" },
  filetypes = { "nix" },
  root_markers = { "flake.nix", ".git" },
  settings = {
    nixd = {
      formatting = { command = { "nixfmt" } },
      nixpkgs = {
        expr = "import (builtins.getFlake (builtins.toString ./.)).inputs.nixpkgs { }",
      },
      options = {
        nixos = {
          expr = "(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<hostname>.options",
        },
      },
    },
  },
})
vim.lsp.enable("nixd")
```

### VS Code and VSCodium

When nixd is launched through the Nix IDE extension, put the patch under the
extension's `nix.serverSettings.nixd` setting:

```json
{
  "nix.serverSettings": {
    "nixd": {
      "formatting": { "command": ["nixfmt"] }
    }
  }
}
```

### Emacs Eglot

Eglot exposes the same `nixd` section through
`eglot-workspace-configuration`, for example:

```elisp
((nil . ((eglot-workspace-configuration
          . (:nixd
             (:options
              (:nixos
               (:expr "(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<hostname>.options"))))))))
```

## Patch format

A patch is a JSON object. Only `$schema`, `formatting`, `options`, `nixpkgs`,
and `diagnostic` are accepted at the top level. Unknown keys and explicit
`null` values are rejected at every level. The `$schema` value, when present,
must be a string.

All fields are optional. Omitting a field is a no-op. Empty nested objects such
as `"formatting": {}`, `"nixpkgs": {}`, and `"diagnostic": {}` are also
no-ops; they do not restore defaults or clear an earlier source.

### `formatting`

`formatting` accepts either a non-empty string shorthand:

```json
{ "formatting": "nixfmt" }
```

or a strict object with an optional string array:

```json
{ "formatting": { "command": ["nixfmt", "--quiet"] } }
```

The string is equivalent to a one-element command array. An omitted `command`
is a no-op. An explicit empty array, `"command": []`, clears the formatter;
formatting requests then return no edits.

### `options`

`options` is a map from arbitrary provider names to strict objects. Every
provider object must contain a non-empty string `expr` and no other keys:

```json
{
  "options": {
    "nixos": {
      "expr": "(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.<hostname>.options"
    },
    "flake-parts": {
      "expr": "(builtins.getFlake (builtins.toString ./.)).debug.options"
    }
  }
}
```

Unlike the other nested fields, supplying `options` replaces the complete
named-provider map from the lower-precedence configuration. Therefore
`"options": {}` intentionally clears every option provider. Individual
providers cannot be partially patched or cleared with an empty expression.

For integrated Home Manager under NixOS, an option expression often ends in:

```nix
.nixosConfigurations.<hostname>.options.home-manager.users.type.getSubOptions []
```

For standalone Home Manager it usually ends in:

```nix
.homeConfigurations."<user>@<hostname>".options
```

### `nixpkgs`

`nixpkgs` is a strict object with one optional string field:

```json
{ "nixpkgs": { "expr": "import <nixpkgs> { }" } }
```

An empty object is a no-op. An explicit empty string,
`"nixpkgs": { "expr": "" }`, intentionally clears the package provider.

### `diagnostic`

`diagnostic` is a strict object with one optional string array:

```json
{ "diagnostic": { "suppress": ["sema-extra-with"] } }
```

An empty object is a no-op. An explicit empty array,
`"diagnostic": { "suppress": [] }`, intentionally clears the suppression
list.

## Troubleshooting expressions

Evaluate option expressions directly in `nix repl` when completion is missing.
Angle-bracket names in the examples are placeholders, not valid Nix syntax.
For example, replace `<hostname>` with a real output name:

```nix
(builtins.getFlake (builtins.toString ./.)).nixosConfigurations.my-host.options
```
