{ ... }:

{
  projectRootFile = "flake.nix";
  programs = {
    clang-format.enable = true;
    nixpkgs-fmt.enable = true;
    black.enable = true;
  };
}
