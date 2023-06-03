{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    mission-control.url = "github:Platonic-Systems/mission-control";
    flake-root.url = "github:srid/flake-root";
  };

  outputs = { nixpkgs, flake-parts, ... }@inputs: flake-parts.lib.mkFlake { inherit inputs; } {
    systems = nixpkgs.lib.systems.flakeExposed;
    imports = [
      inputs.treefmt-nix.flakeModule
      inputs.flake-root.flakeModule
      inputs.mission-control.flakeModule
      inputs.flake-parts.flakeModules.easyOverlay
    ];
    perSystem = { config, self', inputs', pkgs, system, ... }:
      let
        nixd = pkgs.callPackage ./default.nix { };
      in
      {
        treefmt.config = {
          inherit (config.flake-root) projectRootFile;
          programs.clang-format.enable = true;
          package = pkgs.treefmt;
          programs.nixpkgs-fmt.enable = true;
          programs.prettier.enable = true;
        };
        mission-control.scripts = {
          fmt = {
            description = "Format source code";
            exec = config.treefmt.build.wrapper;
            category = "Dev Tools";
          };
        };

        packages.default = nixd;
        overlayAttrs = {
          inherit (config.packages) nixd;
        };
        packages.nixd = nixd;

        devShells.myshell = nixd.overrideAttrs (old: {
          nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.clang-tools pkgs.gdb ];
          shellHook = ''
            export PATH="${pkgs.clang-tools}/bin:$PATH"
            export NIX_SRC=${pkgs.nixUnstable.src}
            export NIX_DEBUG_INFO_DIRS=${pkgs.nixUnstable.debug}/lib/debug
          '';
        });
        devShells.default = pkgs.mkShell {
          inputsFrom = [
            config.flake-root.devShell
            config.mission-control.devShell
            self'.devShells.myshell
          ];
        };
      };
  };
}
