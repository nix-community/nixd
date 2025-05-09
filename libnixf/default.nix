{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  lit,
  gtest,
  boost182,
  nlohmann_json,
  python312,
}:

stdenv.mkDerivation {
  pname = "nixf";
  version = "2.6.4";

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
    python312
  ];

  nativeCheckInputs = [ lit ];

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
