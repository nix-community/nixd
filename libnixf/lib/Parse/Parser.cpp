#include "nixf/Parse/Parser.h"
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/DiagnosticEngine.h"
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

namespace {

bool isKeyword(TokenKind Kind) {
  switch (Kind) {
#define TOK_KEYWORD(KW)                                                        \
  case tok_kw_##KW:                                                            \
    return true;
#include "nixf/Syntax/TokenKinds.inc"
#undef TOK_KEYWORD
  default:
    return false;
  }
}

bool canBeExprStart(TokenKind Kind) {
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

std::string getBracketTokenSpelling(TokenKind Kind) {
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
  case tok_quote2:
    return "''";
  case tok_dquote:
    return "\"";
  default:
    __builtin_unreachable();
  }
}

Diagnostic &diagNullExpr(DiagnosticEngine &Diag, const char *Loc,
                         std::string As) {
  Diagnostic &D = Diag.diag(DK::DK_Expected, OffsetRange(Loc));
  D << ("an expression as " + std::move(As));
  D.fix(Fix::mkInsertion(Loc, " expr"));
  return D;
}

} // namespace

/// Requirements:
///        1. left bracket must exist
///    or  2. left bracket may not exist, but there is some token before it.
///
/// Otherwise the behavior is undefined.
void Parser::matchBracket(TokenKind LeftKind,
                          std::shared_ptr<RawNode> (Parser::*InnerParse)(),
                          TokenKind RightKind) {
  GuardTokensRAII Guard{GuardTokens, RightKind};

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

void Parser::addExprWithCheck(std::string As, std::shared_ptr<RawNode> Expr) {
  assert(LastToken);
  if (Expr)
    Builder.push(std::move(Expr));
  else
    diagNullExpr(Diag, LastToken->getTokEnd(), std::move(As));
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

/// string_part : ( TokStringPart | interpolation | TokStringEscape )*
std::shared_ptr<RawNode> Parser::parseStringParts() {
  Builder.start(SyntaxKind::SK_StringParts);
  while (true) {
    TokenView Tok = peek(0, &Lexer::lexString);
    switch (Tok->getKind()) {
    case tok_dollar_curly: {
      // interpolation, we need to parse a subtree then.
      Builder.push(parseInterpolation());
      continue;
    }
    case tok_string_part:
    case tok_string_escape:
      // If this is a part of string, just push it.
      consume();
      continue;
    case tok_dquote:
    default:
      return Builder.finsih();
    }
  }
}

/// string: '"' string_parts '"'
std::shared_ptr<RawNode> Parser::parseString() {
  Builder.start(SyntaxKind::SK_String);
  matchBracket(tok_dquote, &Parser::parseStringParts, tok_dquote);
  return Builder.finsih();
}

/// ind_string: '' ind_string_parts ''
std::shared_ptr<RawNode> Parser::parseIndString() {
  Builder.start(SyntaxKind::SK_IndString);
  matchBracket(tok_quote2, &Parser::parseIndStringParts, tok_quote2);
  return Builder.finsih();
}

/// ind_string_parts : ( TokStringFragment | interpolation | TokStringEscape )*
std::shared_ptr<RawNode> Parser::parseIndStringParts() {
  Builder.start(SyntaxKind::SK_IndStringParts);
  while (true) {
    const TokenView &Tok = peek(0, &Lexer::lexIndString);
    switch (Tok->getKind()) {
    case tok_dollar_curly: {
      // interpolation, we need to parse a subtree then.
      Builder.push(parseInterpolation());
      continue;
    }
    case tok_string_part:
    case tok_string_escape:
      // If this is a part of string, just push it.
      consume();
      continue;
    default:
      return Builder.finsih();
    }
  }
}

/// path: path_fragment ( path_fragment | interpolation )*
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
    case tok_path_end: {
      consumeOnly();
      goto finish;
    }
    default:
      __builtin_unreachable();
    }
  }
finish:
  return Builder.finsih();
}

/// attrname : identifier
///          | kw_or   <-- "or" used as an identifier
///          | string
///          | interpolation
///
/// \note nullable.
std::shared_ptr<RawNode> Parser::parseAttrName() {
  TokenView Tok = peek();
  switch (Tok->getKind()) {
  case tok_kw_or:
    Diag.diag(DK::DK_OrIdentifier, Tok.getTokRange());
    [[fallthrough]];
  case tok_id:
    consumeOnly();
    return Tok.get();
  case tok_dquote:
    return parseString();
  case tok_dollar_curly:
    return parseInterpolation();
  default:
    return nullptr;
  }
}

/// binding : attrpath '=' expr ';'
/// \note nullable
std::shared_ptr<RawNode> Parser::parseBinding() {
  GuardTokensRAII EqGuard{GuardTokens, tok_eq};
  GuardTokensRAII SemiGuard{GuardTokens, tok_semi_colon};
  // consume n-term `attrpath` and record its range, used for diagnostics.
  const char *AttrBegin = peek().getTokRange().Begin;
  std::shared_ptr<RawNode> AttrPath = parseAttrPath();
  const char *AttrEnd = Lex.cur() - peek()->getLength();

  if (!AttrPath)
    return nullptr;

  assert(LastToken);

  Builder.start(SyntaxKind::SK_Binding);
  Builder.push(std::move(AttrPath));
  OffsetRange AttrRange{AttrBegin, AttrEnd};

  const TokenView &Tok = peek();
  if (Tok->getKind() == tok_eq) {
    consume();
  } else {
    // try to recover from creating unknown node.

    const char *InsPoint = LastToken->getTokEnd();
    const char *UnkBegin = peek().getTokBegin();

    Builder.push(parseUnknownUntilGuard());
    const char *UnkEnd = peek().getTokBegin();
    switch (TokenView Tok = peek(); Tok->getKind()) {
    case tok_eq: {
      // ok, not missing '='
      // attrpath UNKNOWN '=' -> remove UNKNOWN.
      Diagnostic &D = Diag.diag(DK::DK_UnexpectedBetween, {UnkBegin, UnkEnd});
      D << "text";
      D << "attrpath";
      D << "`=`";
      D.note(NK::NK_DeclaresAtHere, AttrRange) << "attrname";
      D.note(NK::NK_DeclaresAtHere, Tok.getTokRange()) << "`=`";
      D.fix(Fix::mkRemoval({UnkBegin, UnkEnd}));
      consume();
      break;
    }
    case tok_semi_colon: {
      // attrpath UNKNOWN ;
      //         ^ insert '='
      // interpret UNKNOWN as normal expression.
      Diagnostic &D = Diag.diag(DK::DK_Expected, Tok.getTokRange());
      D << "=";
      D.note(NK::NK_ToMachThis, AttrRange) << "attrname";
      D.fix(Fix::mkInsertion(InsPoint, " ="));

      // pop Unknown node, guess that it is an expression.
      Builder.pop();
      resetCur(UnkBegin);
      break;
    }
    default: {
      // attrpath UNKNOWN
      // ~~~~~~~~  remove attrpath + UNKNOWN.
      Builder.reset(SyntaxKind::SK_Unknown);
      OffsetRange RM{AttrBegin, UnkEnd};
      Diagnostic &D = Diag.diag(DK::DK_UnexpectedText, RM);
      D << "in binding declaration";
      D.fix(Fix::mkRemoval(RM));
      return Builder.finsih();
    }
    }
  }

  // Carefully dealing with missing ';'
  //
  // See this example (missing 3 semi-colons):
  //
  //     { a = 1 b = 2 c = 3}
  //            ;     ;     ;
  //
  // this will be parsed as { a = (Call 1 b) = 2 }
  //                                        ^ missing semi-colon?
  //
  // However we should guess "b" is an attribute name, not an "expr_select"
  // The following code resolves this issue by saving lex cursor, and re-parse
  // the expression if it is likely a miss-parsed (Call 1 b).

  // Save this cursor, so that we can pop (Call 1 b) and re-parse it
  //
  //  { a = 1 b = 2 c = 3}
  //        ^
  const char *SavedCur = Lex.cur();

  std::shared_ptr<RawNode> Body = parseExpr();

  addExprWithCheck("attr body", Body);
  if (peek()->getKind() == tok_semi_colon) {
    consume(); // ;
  } else {
    // missing ';'
    //  { a = 1 b = 2 c = 3}
    //            ^
    // if the token is '=', guess that we consumed 'b' as (Call 1 b)
    // this should be an attrname.
    if (peek()->getKind() == tok_eq && Body &&
        Body->getSyntaxKind() == SyntaxKind::SK_Call) {
      auto *BodyTwine = dynamic_cast<RawTwine *>(Body.get());
      assert(BodyTwine);
      Builder.pop(); // pop (Call 1 b)
      resetCur(SavedCur);
      assert(Body->getNumChildren() >= 2);
      unsigned Limit = Body->getNumChildren() - 1;
      // Now, parse expr_app with without consuming last expr_select.
      Builder.push(parseExprApp(Limit));
    }

    Diagnostic &D =
        Diag.diag(DK::DK_Expected, OffsetRange{LastToken->getTokEnd()});
    D << "; at the end of binding";
    D.note(NK::NK_DeclaresAtHere, AttrRange) << "attrname";
    D.fix(Fix::mkInsertion(LastToken->getTokEnd(), ";"));
  }
  return Builder.finsih();
}

/// inherit :  'inherit' '(' expr ')' inherited_attrs ';'
///         |  'inherit' inherited_attrs ';'
/// inherited_attrs: attrname*
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
    std::shared_ptr<RawNode> AttrName = parseAttrName();
    if (!AttrName) {
      // This is not an attrname. But we haven't meet the ending ';' yet.
      // Missing semi-colon ?   inherit a b c a = 1;
      //                                       ^ ;
      // TODO: diagnostic.
      break;
    }
    Builder.push(std::move(AttrName));
  }
  return Builder.finsih();
}

/// attrpath : attrname ('.' attrname)*
/// \note nullable
std::shared_ptr<RawNode> Parser::parseAttrPath() {
  std::shared_ptr<RawNode> AttrName = parseAttrName();

  if (!AttrName)
    return nullptr;

  Builder.start(SyntaxKind::SK_AttrPath);
  Builder.push(std::move(AttrName));
  /// Peek '.' and consume next attrname
  while (true) {
    TokenView Tok = peek();
    if (Tok->getKind() != tok_dot)
      break;
    consume();
    assert(LastToken);
    AttrName = parseAttrName();
    if (!AttrName) {
      // not/missing an attrname after
      // guess: extra token '.' ?s
      // attr. = 1
      //     ^ remove this . and continue parsing.
      Diagnostic &D = Diag.diag(Diagnostic::DK_Expected, Tok.getTokRange());
      D << "attrname after `.`";
      D.fix(Fix::mkRemoval(Tok.getTokRange()));
      break;
    }
    Builder.push(std::move(AttrName));
  }
  return Builder.finsih();
}

/// binds : ( binding | inherit )*
std::shared_ptr<RawNode> Parser::parseBinds() {
  Builder.start(SyntaxKind::SK_Binds);
  while (true) {
    TokenView Tok = peek();
    switch (Tok->getKind()) {
    case tok_kw_inherit:
      Builder.push(parseInherit());
      break;
    default: {
      std::shared_ptr<RawNode> Binding = parseBinding();
      if (!Binding)
        goto finish;
      Builder.push(std::move(Binding));
      break;
    }
    }
  }
finish:
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

/// list_body : expr_select*
std::shared_ptr<RawNode> Parser::parseListBody() {
  Builder.start(SyntaxKind::SK_ListBody);
  while (true) {
    std::shared_ptr<RawNode> Expr = parseExprSelect();
    if (Expr)
      Builder.push(std::move(Expr));
    else
      break;
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
  constexpr const char *DefaultExprName = "default expression of the formal";
  assert(peek()->getKind() == tok_id);
  consume();
  assert(LastToken);
  const TokenView &Tok = peek();
  if (Tok->getKind() == tok_question) {
    consume();
    assert(LastToken);
    addExprWithCheck(DefaultExprName);
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
      addExprWithCheck(DefaultExprName);
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
    break;
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
    break;
  default:
    OffsetRange R{LastToken->getTokEnd(), LastToken->getTokEnd()};
    Diagnostic &D = Diag.diag(DK::DK_Expected, R);
    D << ":";
    D.fix(Fix::mkInsertion(LastToken->getTokEnd(), ":"));
  }
  addExprWithCheck("lambda body");
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
  case tok_quote2:
    return parseIndString();
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
  default:
    return nullptr;
  }
}

/// expr_select : expr_simple '.' attrpath
///             | expr_simple '.' attrpath 'or' expr_select
///             | expr_simple 'or' <-- special "apply", 'or' is argument
///             | expr_simple
std::shared_ptr<RawNode> Parser::parseExprSelect() {
  // Firstly consume an expr_simple.
  std::shared_ptr<RawNode> Simple = parseExprSimple();
  if (!Simple)
    return nullptr;
  // Now look-ahead, see if we can find '.'/'or'
  const TokenView &Tok = peek();
  switch (Tok->getKind()) {
  case tok_dot: {
    // expr_simple '.' attrpath
    // expr_simple '.' attrpath 'or' expr_select
    Builder.start(SyntaxKind::SK_Select);
    Builder.push(std::move(Simple));
    consume(); // .
    std::shared_ptr<RawNode> AttrPath = parseAttrPath();
    if (!AttrPath) {
      // guess: extra token '.' ?
      // attr. = 1
      //     ^ remove this . and continue parsing.
      Diagnostic &D = Diag.diag(Diagnostic::DK_Expected, Tok.getTokRange());
      D << "attrpath after `.`";
      D.fix(Fix::mkRemoval(Tok.getTokRange()));
      return Simple;
    }
    Builder.push(std::move(AttrPath));
    if (peek()->getKind() == tok_kw_or) {
      consume();
      Builder.push(parseExprSelect());
    }
    return Builder.finsih();
  }
  case tok_kw_or:
    Builder.start(SyntaxKind::SK_Call);
    Builder.push(std::move(Simple));
    Diag.diag(DK::DK_OrIdentifier, Tok.getTokRange());
    consume();
    return Builder.finsih();
  default:
    // otherwise, end here.
    return Simple;
  }
}
/// expr_app : expr_app expr_select
///            expr_select
std::shared_ptr<RawNode> Parser::parseExprApp(unsigned Limit) {
  std::shared_ptr<RawNode> Simple = parseExprSelect();
  std::vector<std::shared_ptr<RawNode>> V{Simple};
  // Try to consume next expr_select
  for (unsigned I = 1; I < Limit; I++) {
    std::shared_ptr<RawNode> Next = parseExprSelect();
    if (!Next)
      break;
    V.emplace_back(std::move(Next));
  }
  if (V.size() == 1)
    return Simple;
  return std::make_shared<RawTwine>(SyntaxKind::SK_Call, std::move(V));
}

/// Operators.
namespace {

/// Binary operators:
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
std::pair<unsigned, unsigned> getBP(TokenKind Kind) {
  switch (Kind) {
  case tok_op_impl: // %right ->
    return {2, 1};
  case tok_op_or: // %left ||
    return {3, 4};
  case tok_op_and: // %left &&
    return {5, 6};
  case tok_op_eq: // %nonassoc == !=
  case tok_op_neq:
    return {7, 7};
  case tok_op_lt: // %nonassoc < > <= >=
  case tok_op_le:
  case tok_op_ge:
  case tok_op_gt:
    return {8, 8};
  case tok_op_update: // %right //
    return {10, 9};
    // %left NOT - 11
  case tok_op_add: // %left + -
  case tok_op_negate:
    return {12, 13};
  case tok_op_mul: // %left * /
    return {14, 15};
  case tok_op_div:
  case tok_op_concat: // %right ++
    return {17, 16};
  // % op_negate
  default:
    __builtin_unreachable();
  }
}

unsigned getUnaryBP(TokenKind Kind) {
  switch (Kind) {
  case tok_op_not:
    return 11;
  case tok_op_negate:
    return 100;
  default:
    __builtin_unreachable();
  }
}

} // namespace

/// Pratt parser.
/// expr_op : '!' expr_op
///         | '-' expr_op
///         | expr_op BINARY_OP expr_op
std::shared_ptr<RawNode> Parser::parseExprOpBP(unsigned LeftRBP) {
  //
  // expr_op OP           expr_op   OP expr_op
  //            ^LeftRBP  ^       ^ LBP
  //                      |
  //                      | we are here

  // Firstly consume an expression.
  std::shared_ptr<RawNode> Prefix;
  switch (peek()->getKind()) {
  case tok_op_not:
    Builder.start(SyntaxKind::SK_OpNot);
    consume();
    addExprWithCheck("the body of operator `!`",
                     parseExprOpBP(getUnaryBP(tok_op_not)));
    Prefix = Builder.finsih();
    break;
  case tok_op_negate:
    Builder.start(SyntaxKind::SK_OpNegate);
    consume();
    addExprWithCheck("the body of operator `-`",
                     parseExprOpBP(getUnaryBP(tok_op_negate)));
    Prefix = Builder.finsih();
    break;
  default:
    Prefix = parseExprApp();
  }

  if (!Prefix)
    return nullptr;

  assert(LastToken);

  while (true) {
    switch (auto Tok = peek(); Tok->getKind()) {
      // For all binary ops:
      //
      // expr_op OP           expr_op   OP expr_op
      //            ^LeftRBP           ^ LBP
      //                               |
      //                               | we are here
#define TOK_BIN_OP(NAME) case tok_op_##NAME:
#include "nixf/Syntax/TokenKinds.inc"
#undef TOK_BIN_OP
      {
        auto [LBP, RBP] = getBP(Tok->getKind());
        if (LeftRBP <= LBP) {
          if (LeftRBP == LBP) {
            // a == 1 == 2
            // association for non-associable bin-ops.
            // TODO: emit diagnostic
          }
          Builder.start(SyntaxKind::SK_OpBinary);
          Builder.push(Prefix);
          consume(); // bin_op
          if (auto Expr = parseExprOpBP(RBP))
            Builder.push(Expr);
          else
            diagNullExpr(Diag, LastToken->getTokEnd(), "right hand side");
          Prefix = Builder.finsih();
        } else {
          return Prefix;
        }
        break;
      } // case bin_op:
    default:
      return Prefix;
    } // switch
  }   // while(true)
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

std::shared_ptr<RawNode> Parser::parseUnknownUntilGuard() {
  Builder.start(SyntaxKind::SK_Unknown);
  while (true) {
    if (GuardTokens[peek()->getKind()] != 0)
      break;
    consume();
  }
  return Builder.finsih();
}

/// let_in_expr :  'let' binds 'in' expr
std::shared_ptr<RawNode> Parser::parseLetInExpr() {
  GuardTokensRAII InGuard{GuardTokens, tok_kw_in};
  Builder.start(SyntaxKind::SK_Let);
  assert(peek()->getKind() == tok_kw_let);
  const char *LetBegin = peek().getTokBegin();
  consume(); // let
  assert(LastToken);

  Builder.push(parseBinds());

  if (peek()->getKind() == tok_kw_in) {
    consume(); // in
  } else {
    const char *InsPoint = LastToken->getTokEnd();
    std::shared_ptr<RawNode> Expr = parseExpr();
    if (Expr) {
      Builder.push(std::move(Expr));
      // let ... expr
      // missing 'in' ?
      Diagnostic &D = Diag.diag(DK::DK_Expected, OffsetRange{InsPoint});
      D << "`in`";
      D.fix(Fix::mkInsertion(InsPoint, " in "));
      return Builder.finsih();
    }
    // try to recover from creating unknown node.
    const char *UnkBegin = peek().getTokBegin();
    Builder.push(parseUnknownUntilGuard());
    const char *UnkEnd = peek().getTokBegin();

    switch (TokenView Tok = peek(); Tok->getKind()) {
    case tok_kw_in: {
      // ok, not missing 'in'
      // 'let' binds UNKNOWN 'in' -> remove UNKNOWN.
      Diagnostic &D =
          Diag.diag(DK::DK_UnexpectedBetween, OffsetRange{UnkBegin, UnkEnd});
      D << "text";
      D << "let";
      D << "`in`";
      D.note(NK::NK_DeclaresAtHere, Tok.getTokRange()) << "`in`";
      D.fix(Fix::mkRemoval({UnkBegin, UnkEnd}));
      consume();
      break;
    }
    default: {
      // let ... UNKNOWN -> remove all
      Builder.reset(SyntaxKind::SK_Unknown);
      OffsetRange RM{LetBegin, UnkEnd};
      Diagnostic &D = Diag.diag(DK::DK_UnexpectedText, RM);
      D << "in let-in expression";
      D.fix(Fix::mkRemoval(RM));
      return Builder.finsih();
    }
    }
  }

  addExprWithCheck("body");
  return Builder.finsih();
}

/// assert_expr : 'assert' expr ';' expr
std::shared_ptr<RawNode> Parser::parseAssertExpr() {
  assert(peek()->getKind() == tok_kw_assert);
  Builder.start(SyntaxKind::SK_Assert);
  consume(); // assert
  addExprWithCheck("assert cond");

  if (peek()->getKind() == tok_semi_colon) {
    consume(); // ;
  }

  addExprWithCheck("assert cond");

  return Builder.finsih();
}

/// with_expr :  'with' expr ';' expr
std::shared_ptr<RawNode> Parser::parseWithExpr() {
  assert(peek()->getKind() == tok_kw_with);
  Builder.start(SyntaxKind::SK_With);
  consume(); // with
  addExprWithCheck("with cond");

  if (peek()->getKind() == tok_semi_colon) {
    consume(); // ;
  }

  addExprWithCheck("with body");

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
      case tok_question: // { a ?
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
    return parseAssertExpr();
  case tok_kw_with:
    return parseWithExpr();
  case tok_kw_let: {
    switch (peek(1)->getKind()) {
    case tok_l_curly:
      return parseLegacyLet();
    default:
      return parseLetInExpr();
    }
  }
  default:
    return parseExprOp();
  }
}

std::shared_ptr<RawNode> Parser::parse() {
  GuardTokensRAII Guard{GuardTokens, tok_eof};
  Builder.start(SyntaxKind::SK_Root);
  while (true) {
    if (peek()->getKind() == tok::tok_eof) {
      Builder.start(SyntaxKind::SK_EOF);
      consume(); // consume eof.
      Builder.push(Builder.finsih());
      break;
    }
    if (std::shared_ptr<RawNode> Raw = parseExpr()) {
      Builder.push(std::move(Raw));
    } else {
      Builder.start(SyntaxKind::SK_Unknown);
      consume(); // consume this unknown token.
      Builder.push(Builder.finsih());
    }
  }
  return Builder.finsih();
}

} // namespace nixf
