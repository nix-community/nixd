# save this as shell.nix

# $NIX_PATH: nixpkgs=?

{ pkgs ? import <nixpkgs> { } }:

pkgs.callPackage ./default.nix { }
