# RUN: nix-ast-dump %s | FileCheck %s

# CHECK: ExprLet: (let x = 1; in rec { concat = ("a" + "b"); exprAssert = assert true; "some message"; exprDiv = (__div 5 3); exprPos = __curPos; exprWith = (with someAttrSet; [ ]); floatAdd = (1.5 + 2); hasAttr = ((someAttrSet) ? a); ifElse = (if 1 then 1 else 0); integerAnd = (1 && 0); lambda = ({  }: { }); listConcat = ([ (1) (2) ] ++ [ (3) (4) ]); opEq = (1 == 2); opImpl = (1 -> 2); opNEq = (1 != 2); opNot = (! false); opOr = (1 || 2); path = {{.*}}; select = (someAttrSet).a; someAttrSet = { a = 1; }; string = "someString"; update = ({ } // { }); })
let
  # CHECK-NEXT:   ExprAttrs: { x = 1; }
  # CHECK-NEXT: ExprInt: 1
  x = 1;
in
# CHECK-NEXT: ExprAttrs: rec { concat = ("a" + "b"); exprAssert = assert true; "some message"; exprDiv = (__div 5 3); exprPos = __curPos; exprWith = (with someAttrSet; [ ]); floatAdd = (1.5 + 2); hasAttr = ((someAttrSet) ? a); ifElse = (if 1 then 1 else 0); integerAnd = (1 && 0); lambda = ({  }: { }); listConcat = ([ (1) (2) ] ++ [ (3) (4) ]); opEq = (1 == 2); opImpl = (1 -> 2); opNEq = (1 != 2); opNot = (! false); opOr = (1 || 2); path = {{.*}}; select = (someAttrSet).a; someAttrSet = { a = 1; }; string = "someString"; update = ({ } // { }); }
rec {
  # CHECK-NEXT: ExprPath: {{.*}}
  path = ./.;
  # CHECK-NEXT: ExprOpHasAttr: ((someAttrSet) ? a)
  # CHECK-NEXT:   ExprVar: someAttrSet
  hasAttr = someAttrSet ? "a";

  # CHECK-NEXT: ExprAttrs: { a = 1; }
  # CHECK-NEXT:     ExprInt: 1
  someAttrSet = { a = 1; };

  # CHECK-NEXT:    ExprOpAnd: (1 && 0)
  # CHECK-NEXT:     ExprInt: 1
  # CHECK-NEXT:     ExprInt: 0
  integerAnd = 1 && 0;

  # CHECK-NEXT:    ExprConcatStrings: (1.5 + 2)
  # CHECK-NEXT:     ExprFloat: 1.5
  # CHECK-NEXT:     ExprFloat: 2
  floatAdd = 1.5 + 2.0;

  # CHECK-NEXT:    ExprIf: (if 1 then 1 else 0)
  # CHECK-NEXT:     ExprInt: 1
  # CHECK-NEXT:     ExprInt: 1
  # CHECK-NEXT:     ExprInt: 0
  ifElse = if 1 then 1 else 0;

  # CHECK-NEXT:    ExprOpUpdate: ({ } // { })
  # CHECK-NEXT:     ExprAttrs: { }
  # CHECK-NEXT:     ExprAttrs: { }
  update = { } // { };

  # CHECK-NEXT:    ExprOpEq: (1 == 2)
  # CHECK-NEXT:     ExprInt: 1
  # CHECK-NEXT:     ExprInt: 2
  opEq = 1 == 2;

  # CHECK-NEXT:    ExprOpNEq: (1 != 2)
  # CHECK-NEXT:     ExprInt: 1
  # CHECK-NEXT:     ExprInt: 2
  opNEq = 1 != 2;

  # CHECK-NEXT:    ExprOpImpl: (1 -> 2)
  # CHECK-NEXT:     ExprInt: 1
  # CHECK-NEXT:     ExprInt: 2
  opImpl = 1 -> 2;

  # CHECK-NEXT:    ExprOpNot: (! false)
  # CHECK-NEXT:     ExprVar: false
  opNot = ! false;

  # CHECK-NEXT:    ExprOpOr: (1 || 2)
  # CHECK-NEXT:     ExprInt: 1
  # CHECK-NEXT:     ExprInt: 2
  opOr = 1 || 2;

  # CHECK-NEXT:    ExprLambda: ({  }: { })
  # CHECK-NEXT:     ExprAttrs: { }
  lambda = {}: { };

  # CHECK-NEXT:    ExprString: "someString"
  string = "someString";

  # CHECK-NEXT:    ExprOpConcatLists: ([ (1) (2) ] ++ [ (3) (4) ])
  # CHECK-NEXT:     ExprList: [ (1) (2) ]
  # CHECK-NEXT:      ExprInt: 1
  # CHECK-NEXT:      ExprInt: 2
  # CHECK-NEXT:     ExprList: [ (3) (4) ]
  # CHECK-NEXT:      ExprInt: 3
  # CHECK-NEXT:      ExprInt: 4
  listConcat = [ 1 2 ] ++ [ 3 4 ];

  # CHECK-NEXT:    ExprConcatStrings: ("a" + "b")
  # CHECK-NEXT:     ExprString: "a"
  # CHECK-NEXT:     ExprString: "b"
  concat = "a" + "b";

  # CHECK-NEXT:    ExprCall: (__div 5 3)
  # CHECK-NEXT:     ExprInt: 5
  # CHECK-NEXT:     ExprInt: 3
  # CHECK-NEXT:     ExprVar: __div
  exprDiv = 5 / 3;

  # CHECK-NEXT:    ExprPos: __curPos
  exprPos = __curPos;

  # CHECK-NEXT:    ExprAssert: assert true; "some message"
  # CHECK-NEXT:     ExprVar: true
  # CHECK-NEXT:     ExprString: "some message"
  exprAssert = assert true ; "some message";

  # CHECK-NEXT:    ExprSelect: (someAttrSet).a
  # CHECK-NEXT:     ExprVar: someAttrSet
  select = someAttrSet.a;

  # CHECK-NEXT:    ExprWith: (with someAttrSet; [ ])
  # CHECK-NEXT:     ExprVar: someAttrSet
  # CHECK-NEXT:     ExprList: [ ]
  exprWith = with someAttrSet; [ ];
}
