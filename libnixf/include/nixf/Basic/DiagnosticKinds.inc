/// DiagnosticKinds.inc, diagnostic declarations

#ifdef DIAG

DIAG("lex-unterminated-bcomment", UnterminatedBComment, Error,
     "unterminated /* comment")
DIAG("lex-float-no-exp", FloatNoExp, Fatal,
     "float point has trailing `{}` but has no exponential part")
DIAG("lex-float-leading-zero", FloatLeadingZero, Warning,
     "float begins with extra zeros `{}` is nixf extension")
DIAG("let-dynamic", LetDynamic, Error,
     "dynamic attributes are not allowed in let ... in ... expression")
DIAG("inherit-dynamic", InheritDynamic, Error,
     "dynamic attributes are not allowed in inherit")
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
DIAG("dup-formal", DuplicatedFormal, Error,
     "duplicated function formal declaration")
DIAG("dup-formal-arg", DuplicatedFormalToArg, Error,
     "function argument duplicated to a function formal")
DIAG("merge-diff-rec", MergeDiffRec, Warning,
     "merging two attributes with different `rec` modifiers, the latter "
     "will be implicitly ignored")
DIAG("bison", BisonParse, Fatal, "{}")
DIAG("invalid-float", InvalidFloat, Fatal, "invalid float {}")
DIAG("invalid-integer", InvalidInteger, Fatal, "invalid integer {}")
DIAG("dup-formal-arg", DuplicatedAttr, Error, "duplicated attr `{}`")

#endif // DIAG
