{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = { nixpkgs, flake-parts, ... }@inputs: flake-parts.lib.mkFlake { inherit inputs; } {
    imports = [
      inputs.flake-parts.flakeModules.easyOverlay
    ];
    perSystem = { config, self', inputs', pkgs, system, ... }:
      with pkgs;
      let
        nix = nixVersions.nix_2_19;
        llvmPackages = llvmPackages_16;
        nixd = callPackage ./default.nix {
          inherit nix;
          inherit llvmPackages;
        };
        nixdLLVM = nixd.override {
          stdenv = if stdenv.isDarwin then stdenv else llvmPackages.stdenv;
        };
        regressionDeps = with pkgs; [
          clang-tools
          nixpkgs-fmt
        ];
        shellOverride = old: {
          nativeBuildInputs = old.nativeBuildInputs ++ regressionDeps;
          shellHook = ''
            export PATH="${pkgs.clang-tools}/bin:$PATH"
            export NIX_DEBUG_INFO_DIRS=${nix.debug}/lib/debug
            export NIX_SRC=${nix.src}
            export NIX_PATH=nixpkgs=${nixpkgs}
          '';
          hardeningDisable = [ "fortify" ];
        };
      in
      {
        packages.default = nixd;
        overlayAttrs = {
          inherit (config.packages) nixd;
        };
        packages.nixd = nixd;

        devShells.llvm = nixdLLVM.overrideAttrs shellOverride;

        devShells.default = nixd.overrideAttrs shellOverride;

        devShells.nvim = pkgs.mkShell {
          nativeBuildInputs = [
            nixd
            pkgs.nixpkgs-fmt
            (import ./nixd/docs/editors/nvim-lsp.nix { inherit pkgs; })
          ];
        };
        devShells.vscodium = pkgs.mkShell {
          nativeBuildInputs = [
            nixd
            pkgs.nixpkgs-fmt
            (import ./nixd/docs/editors/vscodium.nix { inherit pkgs; })
          ];
        };
      };
    systems = nixpkgs.lib.systems.flakeExposed;
  };
}
