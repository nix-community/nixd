{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = { flake-parts, ... }@inputs: flake-parts.lib.mkFlake { inherit inputs; } {
    perSystem = { config, self', inputs', pkgs, system, ... }:
      let
        nixd = pkgs.callPackage ./default.nix { };
      in
      {
        packages = {
          inherit nixd;
          default = nixd;
        };

        devShells.default = nixd.overrideAttrs (old: {
          nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.clang-tools pkgs.gdb ];
          shellHook = ''
            export NIX_SRC=${pkgs.nixUnstable.src}
            export NIX_DEBUG_INFO_DIRS=${pkgs.nixUnstable.debug}/lib/debug
          '';
        });
      };
    systems = [ "x86_64-linux" "x86_64-darwin" ];
  };
}
