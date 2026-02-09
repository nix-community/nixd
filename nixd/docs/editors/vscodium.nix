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
  cp ${./vscode-settings.json} "$dir/User/settings.json"
  ${codium}/bin/codium --user-data-dir "$dir" "$@"
''
