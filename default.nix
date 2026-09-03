{
  lib,
  stdenv,
  boost,
  gtest,
  lit,
  llvmPackages,
  meson,
  ninja,
  nixComponents,
  pkg-config,
  nlohmann_json,
  python312,
  libxml2,
  zlib,
}:

stdenv.mkDerivation {
  pname = "nixd";
  version = "nightly";

  src = ./.;

  mesonBuildType = "release";

  # Fail the build if the libLLVM dylib ever sneaks back into the closure.
  disallowedRequisites = [ (lib.getLib llvmPackages.llvm) ];

  nativeBuildInputs = [
    meson
    ninja
    python312
    pkg-config
  ];

  nativeCheckInputs = [ lit ];

  buildInputs = [
    nixComponents.nix-main
    nixComponents.nix-expr
    nixComponents.nix-cmd
    nixComponents.nix-flake
    gtest
    boost
    llvmPackages.llvm
    nlohmann_json
    libxml2
    zlib
  ];

  doCheck = !stdenv.isDarwin;

  checkPhase = ''
    runHook preCheck
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
    meson test --print-errorlogs  unit/libnixf/Basic unit/libnixf/Parse unit/libnixt
    runHook postCheck
  '';

  meta = {
    mainProgram = "nixd";
    description = "A feature-rich Nix language server interoperating with C++ nix";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
