/// \file
/// \brief Basic supporting functions for parsing.

#include "Parser.h"

#include "Tokens.h"
#include "nixf/Parse/Parser.h"

using namespace nixf;

Diagnostic &detail::diagNullExpr(std::vector<Diagnostic> &Diags,
                                 LexerCursor Loc, std::string As) {
  Diagnostic &D =
      Diags.emplace_back(Diagnostic::DK_Expected, LexerCursorRange(Loc));
  D << std::move(As) + " expression";
  D.fix("insert dummy expression").edit(TextEdit::mkInsertion(Loc, " expr"));
  return D;
}

void Parser::pushState(ParserState NewState) {
  resetLookAheadBuf();
  State.push(NewState);
}

void Parser::popState() {
  resetLookAheadBuf();
  State.pop();
}

Parser::StateRAII Parser::withState(ParserState NewState) {
  pushState(NewState);
  return {*this};
}

Parser::SyncRAII Parser::withSync(TokenKind Kind) { return {*this, Kind}; }

std::shared_ptr<Node> nixf::parse(std::string_view Src,
                                  std::vector<Diagnostic> &Diags) {
  Parser P(Src, Diags);
  return P.parse();
}

std::shared_ptr<Expr> nixf::Parser::parse() {
  auto Expr = parseExpr();
  collectComments();
  if (Expr)
    registerNode(Expr.get());
  attachAllComments();
  if (Token Tok = peek(); Tok.kind() != tok::tok_eof) {
    // TODO: maybe we'd like to have multiple expressions in a single file.
    // Report an error.
    Diags.emplace_back(Diagnostic::DK_UnexpectedText, Tok.range());
  }
  return Expr;
}

void Parser::resetLookAheadBuf() {
  if (!LookAheadBuf.empty()) {
    Token Tok = LookAheadBuf.front();

    // Reset the lexer cursor at the beginning of the token.
    Lex.setCur(Tok.lCur());
    LookAheadBuf.clear();
  }
}

Token Parser::peek(std::size_t N) {
  while (N >= LookAheadBuf.size()) {
    switch (State.top()) {
    case PS_Expr:
      LookAheadBuf.emplace_back(Lex.lex());
      break;
    case PS_String:
      LookAheadBuf.emplace_back(Lex.lexString());
      break;
    case PS_IndString:
      LookAheadBuf.emplace_back(Lex.lexIndString());
      break;
    case PS_Path:
      LookAheadBuf.emplace_back(Lex.lexPath());
      break;
    }
  }
  return LookAheadBuf[N];
}

std::optional<LexerCursorRange> Parser::consumeAsUnknown() {
  LexerCursor Begin = peek().lCur();
  bool Consumed = false;
  for (Token Tok = peek(); Tok.kind() != tok_eof; Tok = peek()) {
    if (SyncTokens.contains(Tok.kind()))
      break;
    Consumed = true;
    consume();
  }
  if (!Consumed)
    return std::nullopt;
  assert(LastToken && "LastToken should be set after consume()");
  return LexerCursorRange{Begin, LastToken->rCur()};
}

Parser::ExpectResult Parser::expect(TokenKind Kind) {
  auto Sync = withSync(Kind);
  if (Token Tok = peek(); Tok.kind() == Kind) {
    return Tok;
  }
  // UNKNOWN ?
  // ~~~~~~~ consider remove unexpected text
  if (removeUnexpected()) {
    if (Token Tok = peek(); Tok.kind() == Kind) {
      return Tok;
    }
    // If the next token is not the expected one, then insert it.
    // (we have two errors now).
  }
  // expected Kind
  LexerCursor Insert = LastToken ? LastToken->rCur() : peek().lCur();
  Diagnostic &D =
      Diags.emplace_back(Diagnostic::DK_Expected, LexerCursorRange(Insert));
  D << std::string(tok::spelling(Kind));
  D.fix("insert " + std::string(tok::spelling(Kind)))
      .edit(TextEdit::mkInsertion(Insert, std::string(tok::spelling(Kind))));
  return {&D};
}

void Parser::collectComments() {
  const auto &LexerComments = Lex.comments();
  for (const auto &C : LexerComments) {
    PendingComments.push_back(C);
  }
  Lex.clearComments();
}

void Parser::registerNode(Node *N) {
  if (!N)
    return;
  ParsedNodes.push_back({N, N->lCur(), N->rCur()});
  // Recursively register children
  for (Node *Child : N->children()) {
    registerNode(Child);
  }
}

void Parser::attachAllComments() {
  if (ParsedNodes.empty() || PendingComments.empty())
    return;

  // Sort nodes and comments by position
  std::sort(ParsedNodes.begin(), ParsedNodes.end(),
            [](const NodeInfo &A, const NodeInfo &B) {
              return A.StartCur.offset() < B.StartCur.offset();
            });

  std::sort(PendingComments.begin(), PendingComments.end(),
            [](const CommentPtr &A, const CommentPtr &B) {
              return A->range().lCur().offset() < B->range().lCur().offset();
            });

  // Mark which comments have been used
  std::vector<bool> CommentUsed(PendingComments.size(), false);

  // First pass: attach trailing comments
  for (size_t I = 0; I < ParsedNodes.size(); ++I) {
    Node *CurrentNode = ParsedNodes[I].N;
    LexerCursor NodeEnd = ParsedNodes[I].EndCur;

    // Find the start of the next node (if exists)
    LexerCursor NextNodeStart =
        (I + 1 < ParsedNodes.size())
            ? ParsedNodes[I + 1].StartCur
            : LexerCursor::unsafeCreate(INT64_MAX, INT64_MAX, SIZE_MAX);

    // Look for comments after the current node
    for (size_t J = 0; J < PendingComments.size(); ++J) {
      if (CommentUsed[J])
        continue;

      const auto &Comment = PendingComments[J];
      LexerCursor CommentStart = Comment->range().lCur();
      LexerCursor CommentEnd = Comment->range().rCur();

      // Comment must be after the current node
      if (CommentStart.offset() <= NodeEnd.offset())
        continue;

      // If comment is after the next node, stop
      if (CommentStart.offset() >= NextNodeStart.offset())
        break;

      // Trailing comment rules:
      // On the same line as the node, or before the next line starts
      bool IsSameLine = (CommentStart.line() == NodeEnd.line());
      bool IsBeforeNextLine = (CommentEnd.line() == NodeEnd.line());

      if (IsSameLine || IsBeforeNextLine) {
        CurrentNode->addTrailingComment(Comment);
        CommentUsed[J] = true;
      } else {
        // Not on the same line, this is not trailing
        break;
      }
    }
  }

  // Second pass: attach leading comments
  for (size_t I = 0; I < ParsedNodes.size(); ++I) {
    Node *CurrentNode = ParsedNodes[I].N;
    LexerCursor NodeStart = ParsedNodes[I].StartCur;

    // Find the boundary from the previous node (including its trailing
    // comments)
    LexerCursor PrevBoundary = LexerCursor::unsafeCreate(0, 0, 0);

    if (I > 0) {
      Node *PrevNode = ParsedNodes[I - 1].N;
      LexerCursor PrevNodeEnd = ParsedNodes[I - 1].EndCur;

      // Previous boundary = node end + trailing comments region
      if (!PrevNode->trailingComments().empty()) {
        // If there are trailing comments, boundary is after the last one
        PrevBoundary = PrevNode->trailingComments().back()->range().rCur();
      } else {
        // Otherwise, it's the node's end position
        PrevBoundary = PrevNodeEnd;
      }
    }

    // Find comments in the range [PrevBoundary, NodeStart)
    for (size_t J = 0; J < PendingComments.size(); ++J) {
      if (CommentUsed[J])
        continue;

      const auto &Comment = PendingComments[J];
      LexerCursor CommentStart = Comment->range().lCur();
      LexerCursor CommentEnd = Comment->range().rCur();

      // Comment must be after the previous boundary and before current node
      if (CommentStart.offset() <= PrevBoundary.offset())
        continue;

      if (CommentEnd.offset() > NodeStart.offset())
        continue;

      // This is a leading comment
      CurrentNode->addLeadingComment(Comment);
      CommentUsed[J] = true;
    }
  }
}
