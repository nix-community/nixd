# RUN: nixf-ast-dump < %s | FileCheck %s


{
  # CHECK: IndString {{.*}}
  # CHECK-NEXT: Token 3 ''
  a = ''
aasd as = asd sa dsad ;; as;d sa  <- these punctuations should be ignored.
/* not confusing with comments */
# also single-line comments


# CHECK: Interpolation
${

  # This is an interpolation
  {
    #      CHECK:       Path 4
    # CHECK-NEXT:        Token 3 a/
    # CHECK-NEXT:        Token 1 a
    inter = a/a;
  }
}

$${ this is not an expression } also work for escaped dollar

''\${asdasd} and also escpaed dollar
  '';


  # CHECK: IndString 69
  escape-dollar = ''
$${ this is not an expression } also work for escaped dollar
  '';


  #      CHECK:   IndString 13
  # CHECK-NEXT:      Token 3 ''
  # CHECK-NEXT:      IndStringParts 8
  escape-quote2 = ''
''''
  '';
}
