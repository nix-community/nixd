{
  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.xz";

    flake-parts.url = "github:hercules-ci/flake-parts";

    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      nixpkgs,
      flake-parts,
      treefmt-nix,
      ...
    }@inputs:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        inputs.flake-parts.flakeModules.easyOverlay
      ];
      perSystem =
        {
          config,
          pkgs,
          lib,
          ...
        }:
        let
          inherit (pkgs)
            nixVersions
            llvmPackages_21
            callPackage
            stdenv
            ;
          nixComponents = nixVersions.nixComponents_2_33;
          llvmPackages = llvmPackages_21;
          nixf = callPackage ./libnixf { inherit (nixComponents) nix-expr; };
          nixt = callPackage ./libnixt { inherit nixComponents; };
          nixd = callPackage ./nixd {
            inherit nixComponents nixf nixt;
            inherit llvmPackages;
          };
          nixdMono = callPackage ./. { inherit nixComponents llvmPackages; };
          nixdLLVM = nixdMono.override { stdenv = if stdenv.isDarwin then stdenv else llvmPackages.stdenv; };
          regressionDeps = with pkgs; [
            clang-tools
            lit
            nixfmt
          ];
          shellOverride = old: {
            nativeBuildInputs = old.nativeBuildInputs ++ regressionDeps;
            shellHook = ''
              export PATH="${pkgs.clang-tools}/bin:$PATH"
              export NIX_SRC=${nixComponents.src}
              export NIX_PATH=nixpkgs=${nixpkgs}
            '';
            hardeningDisable = [ "fortify" ];
          };
          testShell = callPackage ./nixd/docs/editors/test-shell.nix;
          examplesSrc = ./nixd/docs/examples/NixOS_Home-Manager;
          treefmtEval = treefmt-nix.lib.evalModule pkgs ./treefmt.nix;
        in
        {
          packages.default = nixd;
          overlayAttrs = {
            inherit (config.packages) nixd;
          };
          packages = {
            inherit nixd nixf nixt;
          };

          devShells = {
            llvm = nixdLLVM.overrideAttrs shellOverride;
            default = nixdMono.overrideAttrs shellOverride;
          }
          // lib.genAttrs [ "nvim" "vscodium" ] (
            editor:
            testShell {
              inherit nixd examplesSrc editor;
            }
          );
          formatter = treefmtEval.config.build.wrapper;
        };
      systems = nixpkgs.lib.systems.flakeExposed;
    };
}
