{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = { nixpkgs, flake-parts, ... }@inputs: flake-parts.lib.mkFlake { inherit inputs; } {
    perSystem = { config, self', inputs', pkgs, system, ... }:
      with pkgs;
      let
        llvmPackages = llvmPackages_16;
        libnixf = callPackage ./default.nix {
          inherit llvmPackages;
        };
        regressionDeps = with pkgs; [
          clang-tools
          nixpkgs-fmt
          valgrind
        ];
        shellOverride = old: {
          nativeBuildInputs = old.nativeBuildInputs ++ regressionDeps;
          shellHook = ''
            export PATH="${pkgs.clang-tools}/bin:$PATH"
          '';
        };
      in
      {
        packages.default = libnixf;
        packages.libnixf = libnixf;
        devShells.llvm = libnixf.overrideAttrs (old: {
          nativeBuildInputs = old.nativeBuildInputs ++ [ llvmPackages.clang ];
        });
        devShells.default = libnixf.overrideAttrs shellOverride;
      };
    systems = nixpkgs.lib.systems.flakeExposed;
  };
}
