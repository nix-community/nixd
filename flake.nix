{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = { flake-parts, ... }@inputs: flake-parts.lib.mkFlake { inherit inputs; } {
    perSystem = { config, self', inputs', pkgs, system, ... }:
      with pkgs;
      let
        llvmPackages = llvmPackages_16;
        devInputs = [ clang-tools gdb ];
        nativeBuildInputs = [
          meson
          ninja
          cmake
          pkg-config

          nixUnstable.dev
          boost.dev
          gtest.dev
          llvmPackages.llvm.dev
        ];
        buildInputs = [
          nixUnstable
          gtest

          llvmPackages.llvm.lib
        ];
      in
      {
        devShells.default = mkShell {
          nativeBuildInputs = devInputs ++ nativeBuildInputs;
          inherit buildInputs;
          shellHook = ''
            export NIX_DEBUG_INFO_DIRS=${nixUnstable.debug}/lib/debug
            export NIX_SRC=${nixUnstable.src}
            export NIX_CONFIG_H=${nixUnstable.dev}/include/nix/config.h
            export CXXFLAGS="-include $NIX_CONFIG_H"
          '';
        };
      };
    systems = [ "x86_64-linux" ];
  };
}
