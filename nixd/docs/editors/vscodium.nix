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
  dir="''${XDG_CACHE_HOME:-~/.cache}/nixd-codium"
  ${coreutils}/bin/mkdir -p "$dir/User"
  cat >"$dir/User/settings.json" <<EOF
  {
  "security.workspace.trust.enabled": false,
  "nix.enableLanguageServer": true,
  "nix.serverPath": "nixd",
  }
  EOF
  ${codium}/bin/codium --user-data-dir "$dir" "$@"
''
