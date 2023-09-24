# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s
{
  # CHECK: ExprConcatStrings: (aa + "bbb" + cc)
  s1 = "${aa}bbb${cc}";

  # CHECK: ExprString: "aabbcc"
  s2 = "aabbcc";
}
