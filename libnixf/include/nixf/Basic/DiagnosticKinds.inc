/// DiagnosticKinds.inc, diagnostic declarations

#ifdef DIAG

DIAG("lex-unterminated-bcomment", UnterminatedBComment, Error,
     "unterminated /* comment")
DIAG("lex-float-no-exp", FloatNoExp, Fatal,
     "float point has trailing `{}` but has no exponential part")
DIAG("lex-float-leading-zero", FloatLeadingZero, Warning,
     "float begins with extra zeros `{}` is nixf extension")
DIAG("parse-expected", Expected, Error, "expected {}")
DIAG("parse-int-too-big", IntTooBig, Error,
     "this integer is too big for nix interpreter")
DIAG("parse-redundant-paren", RedundantParen, Warning, "redundant parentheses")
DIAG("parse-attrpath-extra-dot", AttrPathExtraDot, Error,
     "extra `.` at the end of attrpath")
DIAG("parse-select-extra-dot", SelectExtraDot, Error,
     "extra `.` after expression, but missing attrpath")
DIAG("parse-unexpected-between", UnexpectedBetween, Error,
     "unexpected {} between {} and {}")
DIAG("parse-unexpected", UnexpectedText, Error, "unexpected text")
DIAG("parse-missing-sep-formals", MissingSepFormals, Error,
     "missing seperator `,` between two lambda formals")
DIAG("parse-lambda-arg-extra-at", LambdaArgExtraAt, Error,
     "extra `@` for lambda arg")
DIAG("let-dynamic", LetDynamic, Error,
     "dynamic attributes are not allowed in let ... in ... expression")
DIAG("empty-inherit", EmptyInherit, Warning, "empty inherit expression")
DIAG("or-identifier", OrIdentifier, Warning,
     "keyword `or` used as an identifier")
DIAG("deprecated-url-literal", DeprecatedURL, Warning,
     "URL literal is deprecated")
DIAG("deprecated-let", DeprecatedLet, Warning,
     "using deprecated `let' syntactic sugar `let {{..., body = ...}}' -> "
     "(rec {{..., body = ...}}).body'")
DIAG("path-trailing-slash", PathTrailingSlash, Fatal,
     "path has a trailing slash")
DIAG("merge-diff-rec", MergeDiffRec, Warning,
     "merging two attributes with different `rec` modifiers, the latter "
     "will be implicitly ignored")
DIAG("sema-duplicated-attrname", DuplicatedAttrName, Error,
     "duplicated attrname `{}`")
DIAG("sema-dynamic-inherit", DynamicInherit, Error,
     "dynamic attributes are not allowed in inherit")
DIAG("sema-empty-formal", EmptyFormal, Error, "empty formal")
DIAG("sema-formal-missing-comma", FormalMissingComma, Error,
     "missing `,` for lambda formal")
DIAG("sema-formal-extra-ellipsis", FormalExtraEllipsis, Error,
     "extra `...` for lambda formal")
DIAG("sema-misplaced-ellipsis", FormalMisplacedEllipsis, Error,
     "misplaced `...` for lambda formal")
DIAG("sema-dup-formal", DuplicatedFormal, Error, "duplicated function formal")
DIAG("sema-dup-formal-arg", DuplicatedFormalToArg, Error,
     "function argument duplicated to a function formal")
DIAG("sema-undefined-variable", UndefinedVariable, Error,
     "undefined variable `{}`")
DIAG("sema-def-not-used", DefinitionNotUsed, Warning,
     "definition `{}` is not used")
DIAG("sema-extra-rec", ExtraRecursive, Warning,
     "attrset is not necessary to be `rec`ursive ")
DIAG("sema-extra-with", ExtraWith, Warning, "unused `with` expression")

// let a = 1; b = { a = 2; }; with b;  a
// "a" will bind to "let", not "with", that is not very intuitive.
DIAG("sema-escaping-with", EscapingWith, Warning,
     "this variable will escape nested `with`, binding to static name")

#endif // DIAG
