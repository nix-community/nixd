#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Basic/Range.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Parse/Parser.h"

#include "termcolor.hpp"

#include <iostream>
#include <sstream>

nixf::DiagnosticEngine Diags;

// get sources of code on range offset.
// seperated by "\n"
std::string_view getSources(std::string_view Src, nixf::OffsetRange Range) {
  const char *Begin = Range.Begin + Src.begin();
  const char *End = Range.End + Src.begin();

  // Look back until we meet '\n' or beginning of source.
  while (true) {
    if (Begin == Src.begin())
      break;
    if (*Begin == '\n') {
      Begin++; // filter out '\n'
      break;
    }
    Begin--;
  }

  // Do similar thing, checking '\n' and EOF.
  while (true) {
    if (End == Src.end() || *End == '\n')
      break;
    End++;
  }
  return {Begin, End};
}

std::string Ident = "   ";

class ColorStack {
public:
  enum State {
    Normal,
    Delete,
    Diag,
  };

private:
  std::vector<State> Stack;

public:
  void print() {
    for (auto Status : Stack) {
      switch (Status) {
      case Normal:
        std::cerr << termcolor::reset;
        break;
      case Delete:
        std::cerr << termcolor::crossed;
        break;
      case Diag:
        std::cerr << termcolor::cyan;
        break;
      }
    }
  }
  void push(State St) {
    Stack.push_back(St);
    print();
  }
  void pop() {
    Stack.pop_back();
    print();
  }
};

static void printSrc(std::string_view Src, nixf::OffsetRange Range,
                     const std::vector<nixf::Fix> &Fixes) {
  std::string_view SourceSrc = getSources(Src, Range);
  std::string_view SrcDiag(Range.Begin + Src.begin(), Range.End + Src.begin());

  ColorStack Stack;
  using State = ColorStack::State;

  Stack.push(State::Normal);

  for (const char *Cur = SourceSrc.begin(); Cur != SourceSrc.end(); Cur++) {
    for (const nixf::Fix &F : Fixes) {
      if (F.isInsertion())
        continue;
      if (Cur == F.getOldRange().Begin + Src.begin())
        Stack.push(State::Delete);
      if (Cur == F.getOldRange().End + Src.begin())
        Stack.pop();
    }
    if (Cur == SrcDiag.begin())
      Stack.push(State::Diag);
    if (Cur == SrcDiag.end())
      Stack.pop();
    std::cerr << *Cur;
  }
  std::cerr << "\n";

  // Print fixes.
  for (const nixf::Fix &F : Fixes) {
    for (const char *Cur = SourceSrc.begin(); Cur < SourceSrc.end(); Cur++) {
      if (Cur == F.getOldRange().Begin + Src.begin()) {
        std::cerr << termcolor::green << F.getNewText();
        Cur += F.getNewText().length();
      } else {
        std::cerr << " ";
      }
    }
    std::cerr << "\n";
  }
}

static void printNote(std::string_view Src, nixf::Note &Note) {
  std::cerr << termcolor::bold;
  std::cerr << termcolor::magenta << "note: ";
  std::cerr << termcolor::reset;
  std::cerr << termcolor::bold;
  std::cerr << std::string(Note.format()) << "\n";
  std::cerr << termcolor::reset;
  printSrc(Src, Note.range(), {});
}

static void printDiag(std::string_view Src, nixf::Diagnostic &Diag) {
  std::cerr << termcolor::bold;
  switch (Diags.getServerity(Diag.kind())) {
  case nixf::Diagnostic::DS_Fatal:
    std::cerr << termcolor::red << "fatal: ";
    break;
  case nixf::Diagnostic::DS_Error:
    std::cerr << termcolor::red << "error: ";
    break;
  case nixf::Diagnostic::DS_Warning:
    std::cerr << termcolor::yellow << "warning: ";
    break;
  }
  std::cerr << termcolor::reset << termcolor::bold;
  std::cerr << std::string(Diag.format()) << "\n";
  std::cerr << termcolor::reset;
  printSrc(Src, Diag.range(), Diag.getFixes());

  for (const auto &Note : Diag.notes()) {
    printNote(Src, *Note);
  }
}

int main(int argc, char *argv[]) {
  std::stringstream SS;
  SS << std::cin.rdbuf();
  std::string Src = SS.str();
  nixf::Parser P(Src, Diags);

  std::shared_ptr<nixf::RawNode> Expr = P.parseExpr();

  Expr->dumpAST(std::cout);

  // Emit diagnostics.
  for (const std::unique_ptr<nixf::Diagnostic> &Diag : Diags.diags()) {
    printDiag(Src, *Diag);
  }
  return 0;
}
