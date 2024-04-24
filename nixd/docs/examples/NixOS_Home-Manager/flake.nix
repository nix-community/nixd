{
  description = "A simple flake for NixOS and Home Manager";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, home-manager, ... }:
    {
      nixosConfigurations = {
        hostname = nixpkgs.lib.nixosSystem {
          system = "x86_64-linux";
          modules = [
            ({ pkgs, ... }: {
              networking.hostName = "hostname";
              environment.systemPackages = with pkgs; [
                nixd
              ];
            })
          ];
        };
      };

      homeConfigurations = {
        "user@hostname" = home-manager.lib.homeManagerConfiguration {
          pkgs = nixpkgs.legacyPackages.x86_64-linux;
          modules = [
            {
              home.stateVersion = "24.05";
              home.username = "user";
              home.homeDirectory = "/home/user";
            }
            ({ pkgs, ... }: {
              wayland.windowManager.hyprland.enable = true;
            })
          ];
        };
      };
    };
}
