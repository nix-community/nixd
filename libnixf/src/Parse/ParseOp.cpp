/// \file
/// \brief This file implements parsing of operators.

#include "ParserImpl.h"

#include "nixf/Basic/Nodes/Op.h"
#include "nixf/Basic/Range.h"

#include <cassert>

using namespace nixf::tok;
using namespace nixf::detail;

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

namespace nixf {

std::unique_ptr<Expr> Parser::parseExprOpBP(unsigned LeftRBP) {
  std::unique_ptr<Expr> Prefix;
  LexerCursor LCur = lCur();
  switch (Token Tok = peek(); Tok.kind()) {
  case tok_op_not:
  case tok_op_negate: {
    consume();
    assert(LastToken && "consume() should have set LastToken");
    auto O = std::make_unique<Op>(Tok.range(), Tok.kind());
    auto Expr = parseExprOpBP(getUnaryBP(Tok.kind()));
    if (!Expr)
      diagNullExpr(Diags, LastToken->rCur(),
                   "unary operator " + std::string(tok::spelling(Tok.kind())));
    Prefix =
        std::make_unique<ExprUnaryOp>(LexerCursorRange{LCur, LastToken->rCur()},
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
#include "nixf/Basic/TokenKinds.inc"
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
          // TODO: noassoc
        }
        consume();
        assert(LastToken && "consume() should have set LastToken");
        auto O = std::make_unique<Op>(Tok.range(), Tok.kind());
        auto RHS = parseExprOpBP(RBP);
        if (!RHS) {
          diagNullExpr(Diags, LastToken->rCur(), "binary op RHS");
          continue;
        }
        LexerCursorRange Range{Prefix->lCur(), RHS->rCur()};
        Prefix = std::make_unique<ExprBinOp>(Range, std::move(O),
                                             std::move(Prefix), std::move(RHS));
        break;
      }
    case tok_question: {
      // expr_op '?' attrpath
      consume();
      assert(LastToken && "consume() should have set LastToken");
      auto O = std::make_unique<Op>(Tok.range(), Tok.kind());

      std::unique_ptr<AttrPath> Path = parseAttrPath();
      LexerCursorRange Range{Prefix->lCur(), LastToken->rCur()};
      Prefix = std::make_unique<ExprOpHasAttr>(
          Range, std::move(O), std::move(Prefix), std::move(Path));
      break;
    }
    default:
      return Prefix;
    }
  }
}

} // namespace nixf
