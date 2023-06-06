{ stdenv
, meson
, ninja
, cmake
, pkg-config
, lit
, nixUnstable
, boost182
, gtest
, llvmPackages_16
, lib
}:

let
  filterMesonBuild = dir: builtins.filterSource
    (path: type: type != "directory" || baseNameOf path != "build")
    dir;

  llvmPackages = llvmPackages_16;

in
stdenv.mkDerivation {
  pname = "nixd";
  version = "0.0.1";

  src = filterMesonBuild ./.;

  nativeBuildInputs = [
    meson
    ninja
    cmake
    pkg-config

    # Testing only
    lit

    nixUnstable.dev
    boost182.dev
    gtest.dev
    llvmPackages.llvm.dev
    llvmPackages.clang
  ];

  buildInputs = [
    nixUnstable
    gtest
    boost182.dev

    llvmPackages.llvm.lib
  ];

  CXXFLAGS = "-include ${nixUnstable.dev}/include/nix/config.h";

  meta = {
    description = "Nix language server";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = [ ];
    platforms = lib.platforms.unix;
  };
}
