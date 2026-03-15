{
  pkgs,
  nixd,
  editor,
  examplesSrc,
}:
let
  editors = {
    nvim = {
      pkg = pkgs.callPackage ./nvim-lsp.nix { };
      cmd = "nvim-lsp";
    };
    vscodium = {
      pkg = pkgs.callPackage ./vscodium.nix { };
      cmd = "codium-test";
    };
  };

  editorCfg = editors.${editor};
  editorPkg = editorCfg.pkg;
  editorCmd = editorCfg.cmd;

  shellHook = ''
    echo -e "\033[1;31mDuring the first time nixd launches, the flake inputs will be fetched from the internet, this is rather slow.\033[0m"
    export WORK_ROOT=$(mktemp -d -p /tmp nixd-test-XXXXXX)
    export WORK_TEMP="$WORK_ROOT/${baseNameOf examplesSrc}"

    mkdir -p "$WORK_TEMP"
    cp -r ${examplesSrc}/* "$WORK_TEMP/"
    chmod -R u+w "$WORK_TEMP"

    cd "$WORK_TEMP"

    echo -e "\033[1;32mNow, you can edit the nix file by running the following command:\033[0m"
    echo -e "\033[1;33m'${editorCmd} .'\033[0m"
  '';
in
pkgs.mkShell {
  nativeBuildInputs = [
    nixd
    pkgs.nixfmt
    pkgs.git
    editorPkg
  ];
  inherit shellHook;
}
