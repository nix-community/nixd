# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s

{
  # CHECK: ExprString: "asdada\nasdasd\n"
  foo = ''
        asdada
        asdasd
        '';

  # CHECK: ExprConcatStrings: ("asdasd\nasdasdasd\nasdasdasd\n" + a)
  bar = ''
asdasd
asdasdasd
asdasdasd
${a}'';
}
