/// DiagnosticKinds.inc, diagnostic declarations

// provides: DIAG_SIMPLE(SNAME, CNAME, SEVERITY, MESSAGE)
// provides: DIAG_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)
// provides: DIAG_NOTE(SNAME, CNAME, SEVERITY, MESSAGE)
// provides: DIAG_NOTE_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)

//=============================================================================/
// DIAG_SIMPLE(SName, ClassName, Severity, Message)
// "simple" means they have no additional body
//=============================================================================/

#ifdef DIAG_SIMPLE
DIAG_SIMPLE("note-prev", PrevDeclared, Note, "previously declared here")
DIAG_SIMPLE("let-dynamic", LetDynamic, Error,
            "dynamic attributes are not allowed in let ... in ... expression")
DIAG_SIMPLE("inherit-dynamic", InheritDynamic, Error,
            "dynamic attributes are not allowed in inherit")
DIAG_SIMPLE("empty-inherit", EmptyInherit, Warning, "empty inherit expression")
DIAG_SIMPLE("or-identifier", OrIdentifier, Warning,
            "keyword `or` used as an identifier")
DIAG_SIMPLE("deprecated-url-literal", DeprecatedURL, Warning,
            "URL literal is deprecated")
DIAG_SIMPLE("deprecated-let", DeprecatedLet, Warning,
            "using deprecated `let' syntactic sugar `let {..., body = ...}' -> "
            "(rec {..., body = ...}).body'")
DIAG_SIMPLE("path-trailing-slash", PathTrailingSlash, Fatal,
            "path has a trailing slash")
#endif // DIAG_SIMPLE

//=============================================================================/
// DIAG_NOTE(SName, ClassName, Severity, Message)
// "simple", but with additional note support
//=============================================================================/
#ifdef DIAG_NOTE
DIAG_NOTE("dup-formal", DuplicatedFormal, Error,
          "duplicated function formal declaration")
DIAG_NOTE("dup-formal-arg", DuplicatedFormalToArg, Error,
          "function argument duplicated to a function formal")
DIAG_NOTE("merge-diff-rec", MergeDiffRec, Warning,
          "merging two attributes with different `rec` modifiers, the latter "
          "will be implicitly ignored")
#endif // DIAG_NOTE

#define COMMON_METHOD                                                          \
  Severity severity() const override;                                          \
  Kind kind() const override;                                                  \
  std::string_view format() const override;                                    \
  const char *sname() const override;                                          \
  static constexpr const char *message();

//=============================================================================/
// DIAG_BODY(SName, ClassName, Severity, Message, Body)
// diagnostics with special body
//=============================================================================/
#ifdef DIAG_BODY
DIAG_BODY("bison", BisonParse, Fatal, "{0}", {
  DiagBisonParse(RangeIdx Range, std::string Str);
  std::string Str;
  COMMON_METHOD
})
DIAG_BODY("merge-diff-rec-this-rec", ThisRecursive, Note,
          "this attribute set is {0}recursive", {
            DiagThisRecursive(RangeIdx Range, bool Recursive);
            std::string Str;
            COMMON_METHOD
          })
DIAG_BODY("merge-diff-rec-consider", RecConsider, Note,
          "while this attribute set is marked as {0}recursive, it "
          "will be considered as {1}recursive",
          {
            DiagRecConsider(RangeIdx Range, bool MarkedRecursive);
            std::string Str;
            COMMON_METHOD
          })
DIAG_BODY("invalid-float", InvalidFloat, Note, "invalid float {0}", {
  DiagInvalidFloat(RangeIdx Range, std::string_view Content);
  std::string Str;
  COMMON_METHOD
})
DIAG_BODY("invalid-integer", InvalidInteger, Note, "invalid integer {0}", {
  DiagInvalidInteger(RangeIdx Range, std::string_view Content);
  std::string Str;
  COMMON_METHOD
})
#endif // DIAG_BODY

//=============================================================================/
// DIAG_NOTE_BODY(SName, ClassName, Severity, Message, Body)
// diagnostics with special body, but with additional note support
//=============================================================================/
#ifdef DIAG_NOTE_BODY
DIAG_NOTE_BODY("dup-formal-arg", DuplicatedAttr, Error, "duplicated attr `{0}`",
               {
                 DiagDuplicatedAttr(RangeIdx Range, std::string AttrName);
                 std::string Str;
                 COMMON_METHOD
               })
#endif // DIAG_NOTE_BODY

#undef COMMON_METHOD
