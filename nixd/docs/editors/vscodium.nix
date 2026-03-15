{
  pkgs ? import <nixpkgs> { },
}:
with pkgs;
let
  codium = vscode-with-extensions.override {
    vscode = vscodium;
    vscodeExtensions = with vscode-extensions; [
      jnoortheen.nix-ide
    ];
  };
in
writeShellScriptBin "codium-test" ''
  set -e
  dir="''${XDG_CACHE_HOME:-$HOME/.cache}/nixd-codium"
  ${coreutils}/bin/mkdir -p "$dir/User"
  cat >"$dir/User/settings.json" <<EOF
  {
    "security.workspace.trust.enabled": false,
    "nix.enableLanguageServer": true,
    "nix.serverPath": "nixd",
    "nix.serverSettings": {
      "nixd": {
        "nixpkgs": {
          "expr": "import (builtins.getFlake (toString ./.)).inputs.nixpkgs { }"
        },
        "formatting": {
          "command": [ "nixfmt" ]
        },
        "options": {
          "nixos": {
            "expr": "(builtins.getFlake (toString ./.)).nixosConfigurations.hostname.options"
          },
          "home_manager": {
            "expr": "(builtins.getFlake (toString ./.)).homeConfigurations.\"user@hostname\".options"
          },
          "flake_parts": {
            "expr": "let flake = builtins.getFlake (toString ./.); in flake.debug.options // flake.currentSystem.options"
          }
        }
      }
    }
  }
  EOF
  ${codium}/bin/codium --user-data-dir "$dir" "$@"
''
