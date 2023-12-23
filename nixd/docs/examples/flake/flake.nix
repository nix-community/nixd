{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    # use this fork for git repository
    flake-compat = {
      url = "github:inclyc/flake-compat";
      flake = false;
    };
  };

  outputs = { nixpkgs, ... }: {
    foo = nixpkgs.lib.mkIf true;
    #                  ^ goto definition works here (and more)!
  };
}
