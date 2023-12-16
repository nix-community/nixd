#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Basic/Range.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Parse/Parser.h"

#include "termcolor.hpp"

#include <iostream>
#include <sstream>

nixf::DiagnosticEngine Diags;

// get a whole line of source codes where the range offset in.
// seperated by "\n"
std::string_view getSources(std::string_view Src, nixf::OffsetRange Range) {
  const char *Begin = Range.Begin;
  const char *End = Range.End;

  // Look back until we meet '\n' or beginning of source.
  while (Begin > Src.begin()) {
    // Ignoring the '\n' within the range
    // e.g. "a\n"
    if (*Begin == '\n' && Begin != Range.Begin) {
      Begin++; // filter out '\n' itself
      break;
    }
    Begin--;
  }

  // Do similar thing, checking '\n' and EOF.
  while (true) {
    if (End == Src.end())
      break;
    if (*End == '\n') {
      End++;
      break;
    }
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
  std::string_view SrcDiag(Range.Begin, Range.End);

  ColorStack Stack;
  using State = ColorStack::State;

  Stack.push(State::Normal);

  for (const char *Cur = SourceSrc.begin(); Cur != SourceSrc.end(); Cur++) {
    for (const nixf::Fix &F : Fixes) {
      if (F.isInsertion())
        continue;
      if (Cur == F.getOldRange().Begin)
        Stack.push(State::Delete);
      if (Cur == F.getOldRange().End)
        Stack.pop();
    }
    if (Cur == SrcDiag.begin())
      Stack.push(State::Diag);
    if (Cur == SrcDiag.end())
      Stack.pop();
    std::cerr << *Cur;
  }

  for (const nixf::Fix &F : Fixes) {
    std::cerr << termcolor::bold << termcolor::green
              << "fixes: " << termcolor::reset;
    std::string_view FixSrc = getSources(Src, F.getOldRange());
    // a\n\n\n\n\nb
    for (const char *Cur = FixSrc.begin(); Cur < FixSrc.end(); Cur++) {
      if (Cur == F.getOldRange().Begin) {
        std::cerr << termcolor::green << F.getNewText() << termcolor::reset;
        Cur = F.getOldRange().End;
      }
      std::cerr << *Cur;
    }
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

  std::unique_ptr<nixf::RawNode> Expr = P.parse();

  Expr->dumpAST(std::cout);

  // Emit diagnostics.
  for (const std::unique_ptr<nixf::Diagnostic> &Diag : Diags.diags()) {
    printDiag(Src, *Diag);
  }
  return 0;
}
