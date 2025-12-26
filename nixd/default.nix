{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  nixComponents,
  nixf,
  nixt,
  llvmPackages,
  gtest,
  boost,
}:

let
  pname = "nixd";
in
stdenv.mkDerivation {
  inherit pname;
  version = "2.8.1";

  src = ../.;

  outputs = [
    "out"
    "dev"
  ];

  mesonBuildType = "release";

  preConfigure = ''
    cd ${pname}
  '';

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
  ];

  buildInputs = [
    nixComponents.nix-expr
    nixComponents.nix-main
    nixComponents.nix-cmd
    nixf
    nixt
    llvmPackages.llvm
    gtest
    boost
  ];

  meta = {
    mainProgram = "nixd";
    description = "Nix language server";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
