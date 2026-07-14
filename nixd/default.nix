{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  nixComponents,
  nixf,
  nixt,
  llvmPackages,
  gtest,
  boost,
  libxml2,
  zlib,
  llvmStatic ? false,
}:

let
  pname = "nixd";
in
stdenv.mkDerivation {
  inherit pname;
  version = "2.9.1";

  src = ../.;

  outputs = [
    "out"
    "dev"
  ];

  mesonBuildType = "release";

  # Link only LLVM's "support" component statically instead of the
  # monolithic libLLVM dylib; this keeps ~550 MiB of LLVM out of the
  # runtime closure. Static LLVMSupport needs zlib/libxml2 at link time.
  mesonFlags = [ (lib.mesonBool "llvm_static" llvmStatic) ];

  # Fail the build if the libLLVM dylib ever sneaks back into the closure,
  # e.g. because the static link above silently fell back to dynamic.
  disallowedRequisites = lib.optionals llvmStatic [ (lib.getLib llvmPackages.llvm) ];

  preConfigure = ''
    cd ${pname}
  '';

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
  ];

  buildInputs = [
    nixComponents.nix-expr
    nixComponents.nix-main
    nixComponents.nix-cmd
    nixf
    nixt
    llvmPackages.llvm
    gtest
    boost
    libxml2
    zlib
  ];

  meta = {
    mainProgram = "nixd";
    description = "Nix language server";
    homepage = "https://github.com/nix-community/nixd";
    license = lib.licenses.lgpl3Plus;
    maintainers = with lib.maintainers; [ inclyc ];
    platforms = lib.platforms.unix;
  };
}
