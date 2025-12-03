{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  lit,
  gtest,
  boost,
  nlohmann_json,
  python312,
  nix-expr,
}:

stdenv.mkDerivation {
  pname = "nixf";
  version = "nightly";

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
    boost
    nlohmann_json
    nix-expr
  ];

  meta = {
    mainProgram = "nixf-tidy";
    description = "Nix language frontend, parser & semantic analysis";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
