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
          shellHook = ''
            echo -e "\033[1;34mEntering the nvim test environment...\033[0m"
            mkdir -p /tmp/NixOS_Home-Manager
            cp -r ./nixd/docs/examples/NixOS_Home-Manager/* /tmp/NixOS_Home-Manager/
            cd /tmp/NixOS_Home-Manager
            echo -e "\033[1;32mNow, you can edit the nix file by running the following command:\033[0m"
            echo -e "\033[1;33m'nvim-lsp flake.nix'\033[0m"
            echo -e "\033[1;34mEnvironment setup complete.\033[0m"
          '';
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
