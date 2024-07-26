{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  nix,
  gtest,
  boost182,
}:

stdenv.mkDerivation {
  pname = "nixt";
  version = "2.3.0";

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
    nix
    gtest
    boost182
  ];

  env.CXXFLAGS = "-include ${nix.dev}/include/nix/config.h";

  meta = {
    mainProgram = "nixt";
    description = "Nix language frontend, parser & semantic analysis";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
