{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { home-manager, nixpkgs, ... }: rec {
    system = builtins.currentSystem;
    foo = home-manager.lib.homeManagerConfiguration {
      pkgs = nixpkgs.legacyPackages."${system}";
      modules = [
        ({ config, ... }: {

          # <--- Can you see completion lists here?

          # try:

#         home.|
#         xdg.|
#         nixpkgs.|


          home.stateVersion = "22.11";
          home.username = "lyc";
          home.homeDirectory = "/home/lyc";

        })
      ];
    };
  };
}
