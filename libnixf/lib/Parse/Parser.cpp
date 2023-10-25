#include "nixf/Parse/Parser.h"
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Syntax.h"
#include "nixf/Syntax/Token.h"

namespace nixf {

using namespace tok;
using DK = Diagnostic::DiagnosticKind;
using NK = Note::NoteKind;

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

// string: '"' {string_part} '"'
// string : '"' {string_part} '"'
// string_part : STRING_FRAGMENT | interpolation | STRING_ESCAPED
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
  Builder.push(parseAttrPath());
  const TokenView &Tok = peek();
  consume(); // TOOD: diagnostic
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
std::shared_ptr<RawNode> Parser::parseAttrSetExpr() {
  Builder.start(SyntaxKind::SK_AttrSet);
  TokenView Tok = peek();
  if (Tok->getKind() == tok_kw_rec) {
    consume();
  }
  consume();
  Builder.push(parseBinds());
  consume();
  return Builder.finsih();
}

std::shared_ptr<RawNode> Parser::parseExpr() { return parseExprSimple(); }

// simple :  INT
//        | FLOAT
//        | string
//        | indented_string
//        | path
//        | hpath
//        | uri
//        | '(' expr ')'
//        | legacy_let
//        | attrset_expr
//        | list
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
  }
  return nullptr;
}
} // namespace nixf
