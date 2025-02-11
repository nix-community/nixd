from typing import Literal, TypedDict


class Diagnostic(TypedDict):
    sname: str
    "Short name"

    cname: str
    "C++ ideintifer name"

    severity: Literal["Error", "Warning", "Fatal", "Hint"]

    message: str
    "Message of this diagnostic"


diagnostics: list[Diagnostic] = [
    {
        "sname": "lex-unterminated-bcomment",
        "cname": "UnterminatedBComment",
        "severity": "Error",
        "message": "unterminated /* comment",
    },
    {
        "sname": "lex-float-no-exp",
        "cname": "FloatNoExp",
        "severity": "Fatal",
        "message": "float point has trailing `{}` but has no exponential part",
    },
    {
        "sname": "lex-float-leading-zero",
        "cname": "FloatLeadingZero",
        "severity": "Warning",
        "message": "float begins with extra zeros `{}` is nixf extension",
    },
    {
        "sname": "parse-expected",
        "cname": "Expected",
        "severity": "Error",
        "message": "expected {}",
    },
    {
        "sname": "parse-int-too-big",
        "cname": "IntTooBig",
        "severity": "Error",
        "message": "this integer is too big for nix interpreter",
    },
    {
        "sname": "parse-redundant-paren",
        "cname": "RedundantParen",
        "severity": "Warning",
        "message": "redundant parentheses",
    },
    {
        "sname": "parse-attrpath-extra-dot",
        "cname": "AttrPathExtraDot",
        "severity": "Error",
        "message": "extra `.` at the end of attrpath",
    },
    {
        "sname": "parse-select-extra-dot",
        "cname": "SelectExtraDot",
        "severity": "Error",
        "message": "extra `.` after expression, but missing attrpath",
    },
    {
        "sname": "parse-unexpected-between",
        "cname": "UnexpectedBetween",
        "severity": "Error",
        "message": "unexpected {} between {} and {}",
    },
    {
        "sname": "parse-unexpected",
        "cname": "UnexpectedText",
        "severity": "Error",
        "message": "unexpected text",
    },
    {
        "sname": "parse-missing-sep-formals",
        "cname": "MissingSepFormals",
        "severity": "Error",
        "message": "missing seperator `,` between two lambda formals",
    },
    {
        "sname": "parse-lambda-arg-extra-at",
        "cname": "LambdaArgExtraAt",
        "severity": "Error",
        "message": "extra `@` for lambda arg",
    },
    {
        "sname": "parse-operator-noassoc",
        "cname": "OperatorNotAssociative",
        "severity": "Error",
        "message": "operator is non-associative",
    },
    {
        "sname": "parse-kw-or-isnot-binary-op",
        "cname": "KeywordOrIsNotBinaryOp",
        "severity": "Error",
        "message": "'or' keyword is not a binary operator",
    },
    {
        "sname": "let-dynamic",
        "cname": "LetDynamic",
        "severity": "Error",
        "message": "dynamic attributes are not allowed in let ... in ... expression",
    },
    {
        "sname": "empty-inherit",
        "cname": "EmptyInherit",
        "severity": "Warning",
        "message": "empty inherit expression",
    },
    {
        "sname": "or-identifier",
        "cname": "OrIdentifier",
        "severity": "Warning",
        "message": "keyword `or` used as an identifier",
    },
    {
        "sname": "deprecated-url-literal",
        "cname": "DeprecatedURL",
        "severity": "Warning",
        "message": "URL literal is deprecated",
    },
    {
        "sname": "deprecated-let",
        "cname": "DeprecatedLet",
        "severity": "Warning",
        "message": "using deprecated `let' syntactic sugar `let {{..., body = ...}}' -> (rec {{..., body = ...}}).body'",
    },
    {
        "sname": "path-trailing-slash",
        "cname": "PathTrailingSlash",
        "severity": "Fatal",
        "message": "path has a trailing slash",
    },
    {
        "sname": "merge-diff-rec",
        "cname": "MergeDiffRec",
        "severity": "Warning",
        "message": "merging two attributes with different `rec` modifiers, the latter will be implicitly ignored",
    },
    {
        "sname": "sema-duplicated-attrname",
        "cname": "DuplicatedAttrName",
        "severity": "Error",
        "message": "duplicated attrname `{}`",
    },
    {
        "sname": "sema-dynamic-inherit",
        "cname": "DynamicInherit",
        "severity": "Error",
        "message": "dynamic attributes are not allowed in inherit",
    },
    {
        "sname": "sema-empty-formal",
        "cname": "EmptyFormal",
        "severity": "Error",
        "message": "empty formal",
    },
    {
        "sname": "sema-formal-missing-comma",
        "cname": "FormalMissingComma",
        "severity": "Error",
        "message": "missing `,` for lambda formal",
    },
    {
        "sname": "sema-formal-extra-ellipsis",
        "cname": "FormalExtraEllipsis",
        "severity": "Error",
        "message": "extra `...` for lambda formal",
    },
    {
        "sname": "sema-misplaced-ellipsis",
        "cname": "FormalMisplacedEllipsis",
        "severity": "Error",
        "message": "misplaced `...` for lambda formal",
    },
    {
        "sname": "sema-dup-formal",
        "cname": "DuplicatedFormal",
        "severity": "Error",
        "message": "duplicated function formal",
    },
    {
        "sname": "sema-dup-formal-arg",
        "cname": "DuplicatedFormalToArg",
        "severity": "Error",
        "message": "function argument duplicated to a function formal",
    },
    {
        "sname": "sema-undefined-variable",
        "cname": "UndefinedVariable",
        "severity": "Error",
        "message": "undefined variable `{}`",
    },
    {
        "sname": "sema-unused-def-let",
        "cname": "UnusedDefLet",
        "severity": "Warning",
        "message": "definition `{}` in let-expression is not used",
    },
    {
        "sname": "sema-unused-def-lambda-noarg-formal",
        "cname": "UnusedDefLambdaNoArg_Formal",
        "severity": "Warning",
        "message": "attribute `{}` of argument is not used",
    },
    {
        "sname": "sema-unused-def-lambda-witharg-formal",
        "cname": "UnusedDefLambdaWithArg_Formal",
        "severity": "Hint",
        "message": "attribute `{}` of `@`-pattern argument is not used, but may be referenced from the argument",
    },
    {
        "sname": "sema-unused-def-lambda-witharg-arg",
        "cname": "UnusedDefLambdaWithArg_Arg",
        "severity": "Warning",
        "message": "argument `{}` in `@`-pattern is not used",
    },
    {
        "sname": "sema-extra-rec",
        "cname": "ExtraRecursive",
        "severity": "Warning",
        "message": "attrset is not necessary to be `rec`ursive",
    },
    {
        "sname": "sema-extra-with",
        "cname": "ExtraWith",
        "severity": "Warning",
        "message": "unused `with` expression",
    },
]
