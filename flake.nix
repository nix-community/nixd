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
        nixd = callPackage ./default.nix {
          stdenv =
            if stdenv.isDarwin
            then llvmPackages_16.libcxxStdenv
            else stdenv;
        };
      in
      {
        packages.default = nixd;
        overlayAttrs = {
          inherit (config.packages) nixd;
        };
        packages.nixd = nixd;

        devShells.default = nixd.overrideAttrs (old: {
          nativeBuildInputs = old.nativeBuildInputs ++ (with pkgs; [
            clang-tools
            gdb
            nixpkgs-fmt
            llvmPackages_16.clang
          ]);
          shellHook = ''
            export PATH="${pkgs.clang-tools}/bin:$PATH"
            export NIX_SRC=${pkgs.nixUnstable.src}
            export NIX_DEBUG_INFO_DIRS=${pkgs.nixUnstable.debug}/lib/debug
          '';
        });

        devShells.nvim = pkgs.mkShell {
          nativeBuildInputs = [
            nixd
            pkgs.nixpkgs-fmt
            (import ./editors/nvim-lsp.nix { inherit pkgs; })
          ];
        };
        devShells.vscodium = pkgs.mkShell {
          nativeBuildInputs = [
            nixd
            pkgs.nixpkgs-fmt
            (import ./editors/vscodium.nix { inherit pkgs; })
          ];
        };
      };
    systems = nixpkgs.lib.systems.flakeExposed;
  };
}
