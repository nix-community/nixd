/// \file
/// \brief This file implements parsing of operators.

#include "Parser.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Op.h"

#include <cassert>

using namespace nixf::tok;
using namespace nixf::detail;
using namespace nixf;

/// Operators.
namespace {

/// Binary operators:
///
/// %left |> | %right <|
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
  case tok_op_pipe_from:
    return {1, 2};
  case tok_op_pipe_into:
    return {2, 1};
  case tok_op_impl: // %right ->
    return {4, 3};
  case tok_op_or: // %left ||
    return {5, 6};
  case tok_op_and: // %left &&
    return {7, 8};
  case tok_op_eq: // %nonassoc == !=
  case tok_op_neq:
    return {9, 9};
  case tok_op_lt: // %nonassoc < > <= >=
  case tok_op_le:
  case tok_op_ge:
  case tok_op_gt:
    return {10, 10};
  case tok_op_update: // %right //
    return {12, 11};
    // %left NOT - 13
  case tok_op_add: // %left + -
  case tok_op_negate:
    return {14, 15};
  case tok_op_mul: // %left * /
    return {16, 17};
  case tok_op_div:
  case tok_op_concat: // %right ++
    return {19, 18};
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

std::shared_ptr<Expr> Parser::parseExprOpBP(unsigned LeftRBP) {
  std::shared_ptr<Expr> Prefix;
  LexerCursor LCur = lCur();
  switch (Token Tok = peek(); Tok.kind()) {
  case tok_op_not:
  case tok_op_negate: {
    consume();
    assert(LastToken && "consume() should have set LastToken");
    auto O = std::make_shared<Op>(Tok.range(), Tok.kind());
    auto Expr = parseExprOpBP(getUnaryBP(Tok.kind()));
    if (!Expr)
      diagNullExpr(Diags, LastToken->rCur(),
                   "unary operator " + std::string(tok::spelling(Tok.kind())));
    Prefix =
        std::make_shared<ExprUnaryOp>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(O), std::move(Expr));
    break;
  }
  default:
    Prefix = parseExprApp();
  }

  if (!Prefix)
    return nullptr;

  for (;;) {
    switch (Token Tok = peek(); Tok.kind()) {
#define TOK_BIN_OP(NAME) case tok_op_##NAME:
#include "TokenKinds.inc"
#undef TOK_BIN_OP
      {
        // For all binary ops:
        //
        // expr_op OP           expr_op   OP expr_op
        //            ^LeftRBP           ^ LBP
        //                               |
        //                               | we are here
        auto [LBP, RBP] = getBP(Tok.kind());
        if (LeftRBP > LBP)
          return Prefix;
        if (LeftRBP == LBP) {
          // Report error, operator OP and expr_op is not associative.
          Diags.emplace_back(Diagnostic::DK_OperatorNotAssociative,
                             Tok.range());
        }
        consume();
        assert(LastToken && "consume() should have set LastToken");
        auto O = std::make_shared<Op>(Tok.range(), Tok.kind());
        auto RHS = parseExprOpBP(RBP);
        if (!RHS) {
          diagNullExpr(Diags, LastToken->rCur(), "binary op RHS");
          continue;
        }
        LexerCursorRange Range{Prefix->lCur(), RHS->rCur()};
        Prefix = std::make_shared<ExprBinOp>(Range, std::move(O),
                                             std::move(Prefix), std::move(RHS));
        break;
      }
    case tok_question: {
      // expr_op '?' attrpath
      consume();
      assert(LastToken && "consume() should have set LastToken");
      auto O = std::make_shared<Op>(Tok.range(), Tok.kind());

      std::shared_ptr<AttrPath> Path = parseAttrPath();
      LexerCursorRange Range{Prefix->lCur(), LastToken->rCur()};
      Prefix = std::make_shared<ExprOpHasAttr>(
          Range, std::move(O), std::move(Prefix), std::move(Path));
      break;
    }
    default:
      return Prefix;
    }
  }
}
