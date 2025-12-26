{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  gtest,
  boost,
  nixComponents,
}:

stdenv.mkDerivation {
  pname = "nixt";
  version = "2.8.1";

  src = ../.;

  outputs = [
    "out"
    "dev"
  ];

  mesonBuildType = "release";

  preConfigure = ''
    cd libnixt
  '';

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
  ];

  buildInputs = [
    nixComponents.nix-main
    nixComponents.nix-expr
    nixComponents.nix-cmd
    nixComponents.nix-flake
    gtest
    boost
  ];

  meta = {
    mainProgram = "nixt";
    description = "Nix language frontend, parser & semantic analysis";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
