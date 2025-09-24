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
  llvmPackages,
  nixComponents,
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
    boost
    gtest
    llvmPackages.llvm
    nixComponents.nix-cmd
    nixComponents.nix-expr
    nixComponents.nix-main
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
