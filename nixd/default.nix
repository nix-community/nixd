{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  nix,
  nixf,
  nixt,
  llvmPackages,
  gtest,
  boost182,
}:

let
  pname = "nixd";
in
stdenv.mkDerivation {
  inherit pname;
  version = "2.6.0";

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
    nix
    nixf
    nixt
    llvmPackages.llvm
    gtest
    boost182
  ];

  env.CXXFLAGS = "-include ${nix.dev}/include/nix/config.h";

  meta = {
    mainProgram = "nixd";
    description = "Nix language server";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
