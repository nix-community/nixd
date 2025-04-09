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
  version = "nightly";

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

  meta = {
    mainProgram = "nixt";
    description = "Nix language frontend, parser & semantic analysis";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
