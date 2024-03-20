/// \file
/// \brief Parser for the Nix expression language.
#pragma once

#include "Lexer.h"

#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Basic.h"
#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Basic/Nodes/Simple.h"
#include "nixf/Basic/Range.h"

#include <climits>
#include <deque>
#include <set>
#include <stack>

namespace nixf {

namespace detail {

Diagnostic &diagNullExpr(std::vector<Diagnostic> &Diags, LexerCursor Loc,
                         std::string As);

} // namespace detail

using namespace nixf::tok;

class Parser {
public:
  enum ParserState {
    PS_Expr,
    PS_String,
    PS_IndString,
    PS_Path,
  };

private:
  std::string_view Src;
  Lexer Lex;
  std::vector<Diagnostic> &Diags;

  std::deque<Token> LookAheadBuf;
  std::optional<Token> LastToken;
  std::stack<ParserState> State;

  /// \brief Sync tokens for error recovery.
  ///
  /// These tokens will be considered as the end of "unknown" node.
  /// We create "unknown" node for recover from "extra" token error.
  /// (Also, this node is invisible in the AST)
  ///
  /// e.g. { foo....bar = ; }
  ///            ^~~  remove these tokens
  ///
  /// Sync tokens will not be consumed as "unknown".
  std::multiset<TokenKind> SyncTokens;

  class StateRAII {
    Parser &P;

  public:
    StateRAII(Parser &P) : P(P) {}
    ~StateRAII() { P.popState(); }
  };

  // Note: use `auto` for this type.
  StateRAII withState(ParserState NewState);

  /// \brief Reset the lexer cursor to the beginning of the first token.
  ///
  /// This is used for error recovery & context switching.
  void resetLookAheadBuf();

  void pushState(ParserState NewState);

  void popState();

  Token peek(std::size_t N = 0);

  /// \brief Consume tokens until the next sync token.
  /// \returns The consumed range. If no token is consumed, return nullopt.
  std::optional<LexerCursorRange> consumeAsUnknown();

  class SyncRAII {
    Parser &P;
    TokenKind Kind;

  public:
    SyncRAII(Parser &P, TokenKind Kind) : P(P), Kind(Kind) {
      P.SyncTokens.emplace(Kind);
    }
    ~SyncRAII() { P.SyncTokens.erase(P.SyncTokens.find(Kind)); }
  };

  SyncRAII withSync(TokenKind Kind);

  class ExpectResult {
    bool Success;
    std::optional<Token> Tok;
    Diagnostic *DiagMissing;

  public:
    ExpectResult(Token Tok) : Success(true), Tok(Tok), DiagMissing(nullptr) {}
    ExpectResult(Diagnostic *DiagMissing)
        : Success(false), DiagMissing(DiagMissing) {}

    [[nodiscard]] bool ok() const { return Success; }
    [[nodiscard]] Token tok() const {
      assert(Tok);
      return *Tok;
    }
    [[nodiscard]] Diagnostic &diag() const {
      assert(DiagMissing);
      return *DiagMissing;
    }
  };

  ExpectResult expect(TokenKind Kind);

  void consume() {
    if (LookAheadBuf.empty())
      peek(0);
    popBuf();
  }

  Token popBuf() {
    LastToken = LookAheadBuf.front();
    LookAheadBuf.pop_front();
    return *LastToken;
  }

  bool removeUnexpected() {
    if (std::optional<LexerCursorRange> UnknownRange = consumeAsUnknown()) {
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_UnexpectedText, *UnknownRange);
      D.fix("remove unexpected text").edit(TextEdit::mkRemoval(*UnknownRange));
      D.tag(DiagnosticTag::Striked);
      return true;
    }
    return false;
  }

  LexerCursor lCur() { return peek().lCur(); }

  /// Pratt parser for binary/unary operators.
  std::shared_ptr<Expr> parseExprOpBP(unsigned BP);

public:
  Parser(std::string_view Src, std::vector<Diagnostic> &Diags)
      : Src(Src), Lex(Src, Diags), Diags(Diags) {
    pushState(PS_Expr);
  }

  /// \brief Parse interpolations.
  ///
  /// \code
  /// interpolation : "${" expr "}"
  /// \endcode
  std::shared_ptr<Interpolation> parseInterpolation();

  /// \brief Parse paths.
  ///
  /// \code
  ///  path : path_fragment (path_fragment)*  path_end
  /// Context     PS_Expr       PS_Path        PS_Path
  /// \endcode
  ///
  /// The first token, path_fragment is lexed in PS_Expr context, then switch in
  /// "PS_Path" context. The ending token "path_end" shall be poped with context
  /// switching.
  std::shared_ptr<Expr> parseExprPath();

  /// \code
  /// string_part : interpolation
  ///             | STRING_PART
  ///             | STRING_ESCAPE
  /// \endcode
  std::shared_ptr<InterpolatedParts> parseStringParts();

  /// \code
  /// string : " string_part* "
  ///        | '' string_part* ''
  /// \endcode
  std::shared_ptr<ExprString> parseString(bool IsIndented);

  /// \code
  /// '(' expr ')'
  /// \endcode
  std::shared_ptr<ExprParen> parseExprParen();

  /// \code
  /// attrname : ID
  ///          | string
  ///          | interpolation
  /// \endcode
  std::shared_ptr<AttrName> parseAttrName();

  /// \code
  /// attrpath : attrname ('.' attrname)*
  /// \endcode
  std::shared_ptr<AttrPath> parseAttrPath();

  /// \code
  /// binding : attrpath '=' expr ';'
  /// \endcode
  std::shared_ptr<Binding> parseBinding();

  /// \code
  /// inherit :  'inherit' '(' expr ')' inherited_attrs ';'
  ///         |  'inherit' inherited_attrs ';'
  /// inherited_attrs: attrname*
  /// \endcode
  std::shared_ptr<Inherit> parseInherit();

  /// \code
  /// binds : ( binding | inherit )*
  /// \endcode
  std::shared_ptr<Binds> parseBinds();

  /// attrset_expr : REC? '{' binds '}'
  ///
  /// Note: peek `tok_kw_rec` or `tok_l_curly` before calling this function.
  std::shared_ptr<ExprAttrs> parseExprAttrs();

  /// \code
  /// expr_simple :  INT
  ///             | ID
  ///             | FLOAT
  ///             | string
  ///             | indented_string
  ///             | path
  ///             | hpath
  ///             | uri
  ///             | '(' expr ')'
  ///             | legacy_let
  ///             | attrset_expr
  ///             | list
  /// \endcode
  std::shared_ptr<Expr> parseExprSimple();

  /// \code
  /// expr_select : expr_simple '.' attrpath
  ///             | expr_simple '.' attrpath 'or' expr_select
  ///             | expr_simple 'or' <-- special "apply", 'or' is argument
  ///             | expr_simple
  /// \endcode
  std::shared_ptr<Expr> parseExprSelect();

  /// \code
  /// expr_app : expr_app expr_select
  ///          | expr_select
  /// \endcode
  ///
  /// Consume at most \p Limit number of `expr_select` as arguments
  /// e.g. `Fn A1 A2 A3` with Limit = 2 will be parsed as `((Fn A1 A2) A3)`
  std::shared_ptr<Expr> parseExprApp(int Limit = INT_MAX);

  /// \code
  /// expr_list : '[' expr_select* ']'
  /// \endcode
  std::shared_ptr<ExprList> parseExprList();

  /// \code
  /// formal : ,? ID
  ///        | ,? ID '?' expr
  ///        | ,? ...
  /// \endcode
  std::shared_ptr<Formal> parseFormal();

  /// \code
  /// formals : '{' formal* '}'
  /// \endcode
  std::shared_ptr<Formals> parseFormals();

  /// \code
  /// lambda_arg : ID
  ///            | ID @ {' formals '}'
  ///            | '{' formals '}'
  ///            | '{' formals '}' @ ID
  /// \endcode
  std::shared_ptr<LambdaArg> parseLambdaArg();

  /// \code
  /// expr_lambda : lambda_arg ':' expr
  /// \endcode
  std::shared_ptr<ExprLambda> parseExprLambda();

  std::shared_ptr<Expr> parseExpr();

  /// \brief Parse binary/unary operators.
  /// \code
  /// expr_op : '!' expr_op
  ///         | '-' expr_op
  ///         | expr_op BINARY_OP expr_op
  ///         | expr_app
  ///
  /// %right ->
  /// %left ||
  /// %left &&
  /// %nonassoc == !=
  /// %nonassoc < > <= >=
  /// %right //
  /// %left NOT
  /// %left + -
  /// %left * /
  /// %right ++
  /// %nonassoc '?'
  /// %nonassoc NEGATE
  /// \endcode
  std::shared_ptr<Expr> parseExprOp() { return parseExprOpBP(0); }

  /// \code
  /// expr_if : 'if' expr 'then' expr 'else' expr
  /// \endcode
  std::shared_ptr<ExprIf> parseExprIf();

  /// \code
  /// expr_assert : 'assert' expr ';' expr
  /// \endcode
  std::shared_ptr<ExprAssert> parseExprAssert();

  /// \code
  /// epxr_let : 'let' binds 'in' expr
  /// \endcode
  std::shared_ptr<ExprLet> parseExprLet();

  /// \code
  /// expr_with :  'with' expr ';' expr
  /// \endcode
  std::shared_ptr<ExprWith> parseExprWith();

  std::shared_ptr<Expr> parse() { return parseExpr(); }
};

} // namespace nixf
