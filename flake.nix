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
      let
        nixd = pkgs.callPackage ./default.nix { };
      in
      {
        packages.default = nixd;
        overlayAttrs = {
          inherit (config.packages) nixd;
        };
        packages.nixd = nixd;

        devShells.default = nixd.overrideAttrs (old: {
          nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.clang-tools pkgs.gdb pkgs.nixpkgs-fmt ];
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
      };
    systems = nixpkgs.lib.systems.flakeExposed;
  };
}
