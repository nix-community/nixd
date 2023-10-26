#include "nixf/Parse/Parser.h"
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Range.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Syntax.h"
#include "nixf/Syntax/Token.h"
#include <cassert>

namespace nixf {

using namespace tok;
using DK = Diagnostic::DiagnosticKind;
using NK = Note::NoteKind;

static bool isKeyword(TokenKind Kind) {
  switch (Kind) {
#define TOK_KEYWORD(KW)                                                        \
  case tok_kw_##KW:                                                            \
    return true;
#include "nixf/Syntax/TokenKeywords.inc"
#undef TOK_KEYWORD
  default:
    return false;
  }
}

static bool canBeExprStart(TokenKind Kind) {
  switch (Kind) {
  case tok_dot:
  case tok_r_bracket:
  case tok_r_curly:
  case tok_r_paren:
  case tok_eof:

  case tok_ellipsis:
  case tok_comma:
    // tok_dot could be a expr by being a part of .23 (a float)
  case tok_semi_colon:
  case tok_eq:

  case tok_question:
  case tok_at:
  case tok_colon:
    return false;
  default:
    return !isKeyword(Kind) || Kind == tok_kw_or;
  }
}

static std::string getBracketTokenSpelling(TokenKind Kind) {
  switch (Kind) {
  case tok_l_curly:
    return "{";
  case tok_l_bracket:
    return "[";
  case tok_l_paren:
    return "(";
  case tok_r_curly:
    return "}";
  case tok_r_bracket:
    return "]";
  case tok_r_paren:
    return ")";
  default:
    __builtin_unreachable();
  }
}

/// Requirements:
///        1. left bracket must exist
///    or  2. left bracket may not exist, but there is some token before it.
///
/// Otherwise the behavior is undefined.
void Parser::matchBracket(TokenKind LeftKind,
                          std::shared_ptr<RawNode> (Parser::*InnerParse)(),
                          TokenKind RightKind) {
  const std::string LeftStr = getBracketTokenSpelling(LeftKind);
  const std::string RightStr = getBracketTokenSpelling(RightKind);

  if (TokenView LeftTok = peek(); LeftTok->getKind() == LeftKind) {
    consume();

    Builder.push((this->*InnerParse)());

    if (auto const &RightTok = peek(); RightTok->getKind() == RightKind) {
      consume();
    } else {
      assert(LastToken);
      OffsetRange R{LastToken->getTokEnd(), LastToken->getTokEnd()};
      Diagnostic &D = Diag.diag(DK::DK_Expected, R);
      D << RightStr;
      D.note(NK::NK_ToMachThis, LeftTok.getTokRange()) << LeftStr;
      D.fix(Fix::mkInsertion(LastToken->getTokEnd(), RightStr));
    }
  } else {
    assert(LastToken);
    auto &D = Diag.diag(DK::DK_Expected, LeftTok.getTokRange());
    D << LeftStr;

    if (LeftTok->getKind() == RightKind) {
      // sth } -> sth { }
      D.fix(Fix::mkInsertion(LastToken->getTokEnd(), " " + LeftStr));
    } else {
      // sth ?? -> sth { } ??
      D.fix(Fix::mkInsertion(LastToken->getTokEnd(),
                             " " + LeftStr + " " + RightStr));
    }
  }
}

void Parser::diagNullExpr(const std::string &As) {
  assert(LastToken);
  OffsetRange R{LastToken->getTokEnd()};
  Diagnostic &D = Diag.diag(DK::DK_Expected, R);
  D << ("an expression as " + As);
  D.fix(Fix::mkInsertion(LastToken->getTokEnd(), " expr"));
}

void Parser::addExprWithCheck(const std::string &As) {
  if (std::shared_ptr<RawNode> Expr = parseExpr()) {
    Builder.push(Expr);
  } else {
    diagNullExpr(As);
  }
}

/// interpolation : ${ expr }
std::shared_ptr<RawNode> Parser::parseInterpolation() {
  Builder.start(SyntaxKind::SK_Interpolation);
  consume();
  addExprWithCheck("interpolation");
  TokenView Tok = peek();
  if (Tok->getKind() != tok_r_curly) {
    // Error
  } else {
    consume();
  }
  return Builder.finsih();
}

/// string: '"' {string_part} '"'
/// string : '"' {string_part} '"'
/// string_part : STRING_FRAGMENT | interpolation | STRING_ESCAPED
std::shared_ptr<RawNode> Parser::parseString() {
  Builder.start(SyntaxKind::SK_String);
  consume();
  while (true) {
    TokenView Tok = peek(0, &Lexer::lexString);
    bool Finish = false;
    switch (Tok->getKind()) {
    case tok_dquote:
      // end of a string.
      Finish = true;
      consume();
      break;
    case tok_eof:
      // encountered EOF, unbalanced dquote.
      Finish = true;
      break;
    case tok_dollar_curly: {
      // interpolation, we need to parse a subtree then.
      Builder.push(parseInterpolation());
      break;
    }
    case tok_string_part:
      // If this is a part of string, just push it.
      consume();
      break;
    }
    if (Finish)
      break;
  }
  return Builder.finsih();
}

/// path: path_fragment { path_fragment | interpolation }*
std::shared_ptr<RawNode> Parser::parsePath() {
  Builder.start(SyntaxKind::SK_Path);

  assert(peek()->getKind() == tok_path_fragment);
  consume();

  while (true) {
    TokenView Tok = peek(0, &Lexer::lexPath);
    switch (Tok->getKind()) {
    case tok::tok_path_fragment: {
      consume();
      break;
    }
    case tok_dollar_curly: {
      // interpolation, we need to parse a subtree then.
      Builder.push(parseInterpolation());
      break;
    }
    default:
      goto finish;
    }
  }
finish:
  return Builder.finsih();
}

/// attrname : identifier
///          | kw_or   <-- "or" used as an identifier
///          | string
///          | interpolation
std::shared_ptr<RawNode> Parser::parseAttrName() {
  TokenView Tok = peek();
  switch (Tok->getKind()) {
  case tok_kw_or:
  // TODO: Diagnostic.
  case tok_id:
    consumeOnly();
    return Tok.get();
  case tok_dquote:
    return parseString();
  case tok_dollar_curly:
    return parseInterpolation();
  default:
    // TODO: error.
    __builtin_unreachable();
  }
}

/// binding : attrpath '=' expr ';'
std::shared_ptr<RawNode> Parser::parseBinding() {
  Builder.start(SyntaxKind::SK_Binding);
  OffsetRange AttrRange = peek().getTokRange();
  Builder.push(parseAttrPath());
  const TokenView &Tok = peek();
  if (Tok->getKind() == tok_eq) {
    consume();
  } else {
    Diagnostic &D = Diag.diag(DK::DK_Expected, Tok.getTokRange());
    D << "=";
    D.note(NK::NK_ToMachThis, AttrRange) << "attrname";
    assert(LastToken);
    D.fix(Fix::mkInsertion(LastToken->getTokEnd(), " ="));
  }
  addExprWithCheck("attr body");
  consume();
  return Builder.finsih();
}

/// inherit :  'inherit' '(' expr ')' inherited_attrs ';'
///         |  'inherit' inherited_attrs ';'
/// inherited_attrs: {attrname}
std::shared_ptr<RawNode> Parser::parseInherit() {
  Builder.start(SyntaxKind::SK_Inherit);
  consume(); // inherit
  TokenView Tok = peek();
  if (Tok->getKind() == tok_l_paren) {
    consume();
    addExprWithCheck("inherited");
    TokenView Tok2 = peek();
    switch (Tok2->getKind()) {
    case tok::tok_r_paren:
      consume();
      break;
    default:
      // inherit ( expr ??
      // missing )
      Diagnostic &D = Diag.diag(DK::DK_Expected, Tok2.getTokRange());
      D << ")";
      D.note(NK::NK_ToMachThis, Tok.getTokRange()) << "(";
      assert(LastToken);
      D.fix(Fix::mkInsertion(LastToken->getTokEnd(), ")"));
    }
  }

  // parse inherited_attrs.
  while (true) {
    TokenView Tok = peek();
    if (Tok->getKind() == tok_semi_colon) {
      consume();
      break;
    }
    Builder.push(parseAttrName());
  }
  return Builder.finsih();
}

/// attrpath : attrname {. attrname}
std::shared_ptr<RawNode> Parser::parseAttrPath() {
  Builder.start(SyntaxKind::SK_AttrPath);
  Builder.push(parseAttrName());
  /// Peek '.' and consume next attrname
  while (true) {
    TokenView Tok = peek();
    if (Tok->getKind() != tok_dot)
      break;
    consume();
    Builder.push(parseAttrName());
  }
  return Builder.finsih();
}

/// binds : {binding|inherit}
std::shared_ptr<RawNode> Parser::parseBinds() {
  Builder.start(SyntaxKind::SK_Binds);
  while (true) {
    TokenView Tok = peek();
    switch (Tok->getKind()) {
    case tok_kw_inherit:
      Builder.push(parseInherit());
      continue;
    case tok_id:           // identifer
    case tok_dquote:       // string
    case tok_kw_or:        // identifer_or
    case tok_dollar_curly: // interpolation
      Builder.push(parseBinding());
      continue;
    default:
      break;
    }
    break;
  }
  return Builder.finsih();
}

/// attrset_expr : rec? '{' binds '}'
/// Implementation assumption: at least rec or '{'
std::shared_ptr<RawNode> Parser::parseAttrSetExpr() {
  Builder.start(SyntaxKind::SK_AttrSet);
  TokenView Tok = peek();
  if (Tok->getKind() == tok_kw_rec) {
    consume();
  }

  matchBracket(tok_l_curly, &Parser::parseBinds, tok_r_curly);

  return Builder.finsih();
}

/// paren_expr : '(' expr ')'
std::shared_ptr<RawNode> Parser::parseParenExpr() {
  Builder.start(SyntaxKind::SK_Paren);
  matchBracket(tok_l_paren, &Parser::parseExpr, tok_r_paren);
  return Builder.finsih();
}

/// legacy_let: `let` `{` binds `}`
/// let {..., body = ...}' -> (rec {..., body = ...}).body'
///
/// TODO: Fix-it: let {..., body = <body-expr>; }' -> (let ... in <body-expr>)
std::shared_ptr<RawNode> Parser::parseLegacyLet() {
  Builder.start(SyntaxKind::SK_LegacyLet);
  assert(peek()->getKind() == tok_kw_let);
  consume(); // let
  matchBracket(tok_l_curly, &Parser::parseBinds, tok_r_curly);
  return Builder.finsih();
}

/// list_body : {expr_select}
std::shared_ptr<RawNode> Parser::parseListBody() {
  Builder.start(SyntaxKind::SK_ListBody);
  while (true) {
    const TokenView &Tok = peek();
    if (Tok->getKind() == tok_r_bracket || Tok->getKind() == tok_eof)
      break;
    Builder.push(parseExprSelect());
  }
  return Builder.finsih();
}

/// list : '[' list_body ']'
std::shared_ptr<RawNode> Parser::parseListExpr() {
  Builder.start(SyntaxKind::SK_List);
  assert(peek()->getKind() == tok_l_bracket);
  matchBracket(tok_l_bracket, &Parser::parseListBody, tok_r_bracket);
  return Builder.finsih();
}

/// formal : ID
///        | ID '?' expr
std::shared_ptr<RawNode> Parser::parseFormal() {
  Builder.start(SyntaxKind::SK_Formal);
  assert(peek()->getKind() == tok_id);
  consume();
  assert(LastToken);
  const TokenView &Tok = peek();
  if (Tok->getKind() == tok_question) {
    consume();
    assert(LastToken);
    if (canBeExprStart(peek()->getKind())) {
      Builder.push(parseExpr());
    } else {
      // a ? ,
      //    ^  missing expression?
      diagNullExpr("default expression of the formal");
    }
  } else if (canBeExprStart(Tok->getKind())) {
    if (Tok->getKind() != tok_id || peek(1)->getKind() == tok_dot) {
      // expr_start && (!id || (id && id.))
      // guess that missing ? between formal and expression.
      // { a   1
      //     ^
      Diagnostic &D = Diag.diag(Diagnostic::DK_Expected,
                                OffsetRange{LastToken->getTokEnd()});
      D << "?";
      D.note(Note::NK_DeclaresAtHere, LastToken->getTokRange()) << "formal";
      D.fix(Fix::mkInsertion(LastToken->getTokEnd(), " ? "));
      Builder.push(parseExpr());
    }
  }
  return Builder.finsih();
}

/// formals : formal ',' formals
///         | formal
///         |
///         | '...'
/// { a, ... }
/// { a b
/// { a ? 1
/// { a ;
std::shared_ptr<RawNode> Parser::parseFormals() {
  Builder.start(SyntaxKind::SK_Formals);
  TokenView Tok1 = peek();
  switch (Tok1->getKind()) {
  case tok_id: {
    Builder.push(parseFormal());
    TokenView Tok2 = peek();
    switch (Tok2->getKind()) {
    case tok_comma:
      consume();
      Builder.push(parseFormals());
      break;
    case tok_id: {
      Diagnostic &D = Diag.diag(Diagnostic::DK_MissingSepFormals,
                                OffsetRange{Tok1.getTokEnd()});
      D.note(Note::NK_DeclaresAtHere, Tok1.getTokRange()) << "first formal";
      D.note(Note::NK_DeclaresAtHere, Tok2.getTokRange()) << "second formal";
      D.fix(Fix::mkInsertion(Tok1.getTokEnd(), ","));
      Builder.push(parseFormals());
      break;
    }
    default:
      break;
    }
    break;
  }
  case tok_ellipsis:
    consume();
    break;
  default:
    break;
  }
  return Builder.finsih();
}

std::shared_ptr<RawNode> Parser::parseBracedFormals() {
  Builder.start(SyntaxKind::SK_BracedFormals);
  matchBracket(tok_l_curly, &Parser::parseFormals, tok_r_curly);
  return Builder.finsih();
}

/// lambda_arg : ID
///            | ID @ {' formals '}'
///            | '{' formals '}'
///            | '{' formals '}' @ ID
///
/// Assumption: must exists ID or '{'
std::shared_ptr<RawNode> Parser::parseLambdaArg() {
  Builder.start(SyntaxKind::SK_LambdaArg);
  switch (peek()->getKind()) {
  case tok_id: {
    consume();
    if (peek()->getKind() == tok_at) {
      consume();
      Builder.push(parseBracedFormals());
    }
    break;
  }
  case tok_l_curly: {
    Builder.push(parseBracedFormals());
    if (peek()->getKind() == tok_at) {
      consume();
      assert(LastToken);
      switch (peek()->getKind()) {
      case tok_id:
        consume();
        break;
      default:
        OffsetRange R{LastToken->getTokEnd()};
        Diagnostic &D = Diag.diag(DK::DK_Expected, R);
        D << "an identifier";
        D.fix(Fix::mkInsertion(LastToken->getTokEnd(), "arg"));
      }
    }
  }
  default:
    __builtin_unreachable();
  }

  return Builder.finsih();
}

/// lambda_expr : lambda_arg ':' expr
///
/// Assumption: must exists ID or '{'
std::shared_ptr<RawNode> Parser::parseLambdaExpr() {
  Builder.start(SyntaxKind::SK_Lambda);

  // lambda_arg ':' expr
  // ^
  Builder.push(parseLambdaArg());
  assert(LastToken);

  // lambda_arg ':' expr
  //             ^
  switch (peek()->getKind()) {
  case tok_colon:
    consume();
    Builder.push(parseExpr());
    break;
  default:
    OffsetRange R{LastToken->getTokEnd(), LastToken->getTokEnd()};
    Diagnostic &D = Diag.diag(DK::DK_Expected, R);
    D << ":";
    D.fix(Fix::mkInsertion(LastToken->getTokEnd(), ":"));
    Builder.push(parseExpr());
  }

  return Builder.finsih();
}

/// simple :  INT
///        | FLOAT
///        | string
///        | indented_string
///        | path
///        | hpath
///        | uri
///        | '(' expr ')'
///        | legacy_let
///        | attrset_expr
///        | list
std::shared_ptr<RawNode> Parser::parseExprSimple() {
  TokenView Tok = peek();
  switch (Tok->getKind()) {
  case tok_int:
  case tok_float:
  case tok_id:
    consumeOnly();
    return Tok.get();
  case tok_dquote:
    return parseString();
  case tok_l_curly:
  case tok_kw_rec:
    return parseAttrSetExpr();
  case tok_path_fragment:
    return parsePath();
  case tok_l_paren:
    return parseParenExpr();
  case tok_kw_let:
    return parseLegacyLet();
  case tok_l_bracket:
    return parseListExpr();
  }
  return nullptr;
}

/// expr_select : expr_simple '.' attrpath
///             | expr_simple '.' attrpath 'or' expr_select
///             | expr_simple 'or' <-- special "apply", 'or' is argument
///             | expr_simple
std::shared_ptr<RawNode> Parser::parseExprSelect() {
  // Firstly consume an expr_simple.
  std::shared_ptr<RawNode> Simple = parseExprSimple();
  // Now look-ahead, see if we can find '.'/'or'
  const TokenView &Tok = peek();
  switch (Tok->getKind()) {
  case tok_dot:
    // expr_simple '.' attrpath
    // expr_simple '.' attrpath 'or' expr_select
    Builder.start(SyntaxKind::SK_Select);
    Builder.push(Simple);
    consume();
    Builder.push(parseAttrPath());
    if (peek()->getKind() == tok_kw_or) {
      consume();
      Builder.push(parseExprSelect());
    }
    return Builder.finsih();
  case tok_kw_or:
    // `or` used as an identifier.
    // TODO: create a function.
  default:
    // otherwise, end here.
    return Simple;
  }
}
/// expr_app : expr_select expr_app
///            expr_select
std::shared_ptr<RawNode> Parser::parseExprApp() {
  std::shared_ptr<RawNode> Simple = parseExprSelect();

  // Peek ahead to see if we have extra token.
  if (canBeExprStart(peek()->getKind())) {
    Builder.start(SyntaxKind::SK_Call);
    Builder.push(Simple);
    Builder.push(parseExprApp());
    return Builder.finsih();
  }

  return Simple;
}

// TODO: Pratt parser.
std::shared_ptr<RawNode> Parser::parseExprOp() {
  return Parser::parseExprApp();
}

/// if_expr : 'if' expr 'then' expr 'else' expr
std::shared_ptr<RawNode> Parser::parseIfExpr() {
  Builder.start(SyntaxKind::SK_If);
  assert(peek()->getKind() == tok_kw_if);
  consume(); // 'if'
  assert(LastToken);

  addExprWithCheck("`if` body");

  // then?
  if (peek()->getKind() == tok_kw_then) {
    consume(); // then

    // 'if' expr 'then' expr 'else' expr
    //                  ^
    addExprWithCheck("`then` body");
  } else {
    // if ... ??
    // missing 'then' ?
    Diagnostic &D =
        Diag.diag(DK::DK_Expected, OffsetRange{LastToken->getTokEnd()});
    D << "`then`";
    D.fix(Fix::mkInsertion(LastToken->getTokEnd(), " then "));
  }

  // else?
  if (peek()->getKind() == tok_kw_else) {
    consume(); // else
    // 'if' expr 'then' expr 'else' expr
    //                  ^
    addExprWithCheck("`else` body");
  } else {
    // if ... then ... ???
    // missing 'else' ?
    Diagnostic &D =
        Diag.diag(DK::DK_Expected, OffsetRange{LastToken->getTokEnd()});
    D << "`else`";
    D.fix(Fix::mkInsertion(LastToken->getTokEnd(), " else "));
  }
  return Builder.finsih();
}

/// expr      : lambda_expr
///           | assert_expr
///           | with_expr
///           | let_in_expr
///           | if_expr
///           | expr_op
///
/// lambda_expr : ID ':' expr
///             | '{' formals '}' ':' expr
///             | '{' formals '}' '@' ID ':' expr
///             | ID '@' '{' formals '}' ':' expr
///
/// if_expr : 'if' expr 'then' expr 'else' expr
///
/// assert_expr : 'assert' expr ';' expr
///
/// with_expr :  'with' expr ';' expr
///
/// let_in_expr :  'let' binds 'in' expr
std::shared_ptr<RawNode> Parser::parseExpr() {
  // { a }
  // { a ,
  // { a ...  without a ','
  // { a @
  //
  switch (peek()->getKind()) {
  case tok_id: {
    switch (peek(1)->getKind()) {
    case tok_at:    // ID '@'
    case tok_colon: // ID ':'
      return parseLambdaExpr();
    default: // ID ???
      return parseExprOp();
    }
  }
  case tok_l_curly: {
    switch (peek(1)->getKind()) {
    case tok_id: { // { a
      switch (peek(2)->getKind()) {
      case tok_comma:    // { a ,
      case tok_ellipsis: // { a ...
      case tok_r_curly:  // { a }
      case tok_at:       // { a @
        return parseLambdaExpr();
      default:
        goto attrset;
      }
    }
    case tok_r_curly: { // { }
      switch (peek(2)->getKind()) {
      case tok_colon: // { } :
      case tok_at:    // { } @
        return parseLambdaExpr();
      default: // { } ???
        goto attrset;
      }
    }
    default: // { ???
    attrset:
      return parseAttrSetExpr();
    }
  }

  case tok_kw_if:
    return parseIfExpr();
  case tok_kw_assert:
    // return parseAssertExpr();
  case tok_kw_with:
    // return parseWithExpr();
  case tok_kw_let: {
    switch (peek(1)->getKind()) {
    case tok_l_curly:
      return parseLegacyLet();
    default:
      break;
      // return parseLetInExpr();
    }
  }
  default:
    return parseExprOp();
  }
  __builtin_unreachable();
}

std::shared_ptr<RawNode> Parser::parse() {
  Builder.start(SyntaxKind::SK_Root);
  while (true) {
    if (peek()->getKind() == tok::tok_eof)
      break;
    if (std::shared_ptr<RawNode> Raw = parseExpr()) {
      Builder.push(Raw);
    } else {
      Builder.start(SyntaxKind::SK_Unknown);
      consume(); // consume this unknown token.
      Builder.push(Builder.finsih());
    }
  }
  return Builder.finsih();
}

} // namespace nixf
