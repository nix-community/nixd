{
  description = "A simple flake for NixOS and Home Manager using flake-parts";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs =
    inputs@{ self, flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      debug = true;

      systems = [ "x86_64-linux" ];

      perSystem = { config, ... }: { };

      flake = {
        nixosConfigurations = {
          hostname = inputs.nixpkgs.lib.nixosSystem {
            system = "x86_64-linux";
            modules = [
              (
                { pkgs, ... }:
                {
                  networking.hostName = "hostname";
                  environment.systemPackages = with pkgs; [
                    nixd
                  ];
                }
              )
            ];
          };
        };

        homeConfigurations = {
          "user@hostname" = inputs.home-manager.lib.homeManagerConfiguration {
            pkgs = inputs.nixpkgs.legacyPackages.x86_64-linux;
            modules = [
              {
                home.stateVersion = "24.05";
                home.username = "user";
                home.homeDirectory = "/home/user";
              }
              (
                { pkgs, ... }:
                {
                  wayland.windowManager.hyprland.enable = true;
                }
              )
            ];
          };
        };
      };
    };
}
