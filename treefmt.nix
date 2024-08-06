{ ... }:

{
  projectRootFile = "flake.nix";
  programs = {
    clang-format.enable = true;
    nixfmt.enable = true;
    black.enable = true;
  };
}
