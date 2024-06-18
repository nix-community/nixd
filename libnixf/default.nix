{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  lit,
  nixpkgs-fmt,
  gtest,
  boost182,
  nlohmann_json,
}:

stdenv.mkDerivation {
  pname = "nixf";
  version = "2.2.1";

  src = ../.;

  outputs = [
    "out"
    "bin"
    "dev"
  ];

  mesonBuildType = "release";

  preConfigure = ''
    cd libnixf
  '';

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
  ];

  nativeCheckInputs = [
    lit
    nixpkgs-fmt
  ];

  buildInputs = [
    gtest
    boost182
    nlohmann_json
  ];

  meta = {
    mainProgram = "nixf";
    description = "Nix language frontend, parser & semantic analysis";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
