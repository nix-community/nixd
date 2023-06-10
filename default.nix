{ stdenv
, meson
, ninja
, pkg-config
, lit
, boost182
, gtest
, llvmPackages_16
, libbacktrace
, lib
, nixVersions
, nixpkgs-fmt
}:

let
  llvmPackages = llvmPackages_16;
  nix = nixVersions.nix_2_16;
in
stdenv.mkDerivation {
  pname = "nixd";
  version = "nightly";

  src = ./.;

  mesonBuildType = "release";

  nativeBuildInputs = [
    meson
    ninja
    pkg-config

    # For C++ 20 library features on darwin devices.
    llvmPackages.clang
  ];

  nativeCheckInputs = [
    lit
    nixpkgs-fmt
  ];

  buildInputs = [
    libbacktrace
    nix
    gtest
    boost182

    llvmPackages.llvm
  ];

  CXXFLAGS = "-include ${nix.dev}/include/nix/config.h";

  doCheck = true;

  checkPhase = ''
    dirs=(store var var/nix var/log/nix etc home)

    for dir in $dirs; do
      mkdir -p "$TMPDIR/$dir"
    done

    export NIX_STORE_DIR=$TMPDIR/store
    export NIX_LOCALSTATE_DIR=$TMPDIR/var
    export NIX_STATE_DIR=$TMPDIR/var/nix
    export NIX_LOG_DIR=$TMPDIR/var/log/nix
    export NIX_CONF_DIR=$TMPDIR/etc
    export HOME=$TMPDIR/home

    # Disable nixd regression tests, because it uses some features provided by
    # nix, and does not correctly work in the sandbox
    meson test --print-errorlogs server regression/nix-ast-dump
  '';

  meta = {
    description = "Nix language server";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
