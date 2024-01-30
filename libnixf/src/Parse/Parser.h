/// \file
/// \brief Parser for the Nix expression language.
#pragma once

#include "Lexer.h"

#include "nixf/Basic/Nodes.h"

#include <climits>
#include <deque>
#include <set>
#include <stack>

namespace nixf {

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

  class StateRAII;

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

  class SyncRAII;

  SyncRAII withSync(TokenKind Kind);

  class ExpectResult;

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
  std::unique_ptr<Interpolation> parseInterpolation();

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
  std::unique_ptr<Expr> parseExprPath();

  /// \code
  /// string_part : interpolation
  ///             | STRING_PART
  ///             | STRING_ESCAPE
  /// \endcode
  std::unique_ptr<InterpolatedParts> parseStringParts();

  /// \code
  /// string : " string_part* "
  ///        | '' string_part* ''
  /// \endcode
  std::unique_ptr<ExprString> parseString(bool IsIndented);

  /// \code
  /// '(' expr ')'
  /// \endcode
  std::unique_ptr<ExprParen> parseExprParen();

  /// \code
  /// attrname : ID
  ///          | string
  ///          | interpolation
  /// \endcode
  std::unique_ptr<AttrName> parseAttrName();

  /// \code
  /// attrpath : attrname ('.' attrname)*
  /// \endcode
  std::unique_ptr<AttrPath> parseAttrPath();

  /// \code
  /// binding : attrpath '=' expr ';'
  /// \endcode
  std::unique_ptr<Binding> parseBinding();

  /// \code
  /// inherit :  'inherit' '(' expr ')' inherited_attrs ';'
  ///         |  'inherit' inherited_attrs ';'
  /// inherited_attrs: attrname*
  /// \endcode
  std::unique_ptr<Inherit> parseInherit();

  /// \code
  /// binds : ( binding | inherit )*
  /// \endcode
  std::unique_ptr<Binds> parseBinds();

  /// attrset_expr : REC? '{' binds '}'
  ///
  /// Note: peek `tok_kw_rec` or `tok_l_curly` before calling this function.
  std::unique_ptr<ExprAttrs> parseExprAttrs();

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
  std::unique_ptr<Expr> parseExprSimple();

  /// \code
  /// expr_select : expr_simple '.' attrpath
  ///             | expr_simple '.' attrpath 'or' expr_select
  ///             | expr_simple 'or' <-- special "apply", 'or' is argument
  ///             | expr_simple
  /// \endcode
  std::unique_ptr<Expr> parseExprSelect();

  /// \code
  /// expr_app : expr_app expr_select
  ///          | expr_select
  /// \endcode
  ///
  /// Consume at most \p Limit number of `expr_select` as arguments
  /// e.g. `Fn A1 A2 A3` with Limit = 2 will be parsed as `((Fn A1 A2) A3)`
  std::unique_ptr<Expr> parseExprApp(int Limit = INT_MAX);

  /// \code
  /// expr_list : '[' expr_select* ']'
  /// \endcode
  std::unique_ptr<ExprList> parseExprList();

  std::unique_ptr<Expr> parseExpr() {
    return parseExprApp(); // TODO!
  }
  std::unique_ptr<Expr> parse() { return parseExpr(); }
};

} // namespace nixf
