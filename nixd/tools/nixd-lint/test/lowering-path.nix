# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s

{
  # CHECK: lowering-path.nix
  a = ./.;

  # CHECK: ExprCall: (__findFile __nixPath "nixpkgs")
  spath = <nixpkgs>;

  # CHECK-NOT: ~/foo.nix
  # CHECK: foo.nix
  hpath = ~/foo.nix;
}
