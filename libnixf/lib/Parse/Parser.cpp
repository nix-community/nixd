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

/// interpolation : ${ expr }
std::shared_ptr<RawNode> Parser::parseInterpolation() {
  Builder.start(SyntaxKind::SK_Interpolation);
  consume();
  Builder.push(parseExpr());
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
  Builder.push(parseExpr());
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
    Builder.push(parseExpr());
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

std::shared_ptr<RawNode> Parser::parseExpr() { return parseExprSelect(); }

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
} // namespace nixf
