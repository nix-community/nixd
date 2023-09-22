#include "nixd/Expr/Expr.h"
#include "nixd/Sema/EvalContext.h"
#include "nixd/Sema/Lowering.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Parser.h"
#include "nixd/Syntax/Parser/Require.h"

#include <nix/ansicolor.hh>
#include <nix/error.hh>
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FormatVariadic.h>

#include <filesystem>
#include <iostream>
#include <memory>

using namespace llvm::cl;

OptionCategory Misc("miscellaneous options");

opt<std::string> Filename(Positional, desc("<input file>"), init("-"),
                          cat(Misc));

const OptionCategory *Cat[] = {&Misc};

opt<bool> DumpNixAST("dump-nix-ast", init(false),
                     desc("Dump the lowered nix AST"), cat(Misc));

opt<bool> ShowPosition("show-position", init(false),
                       desc("Print location information"), cat(Misc));

static void printCodeLines(std::ostream &Out, const std::string &Prefix,
                           std::shared_ptr<nix::AbstractPos> BeginPos,
                           std::shared_ptr<nix::AbstractPos> EndPos,
                           const nix::LinesOfCode &LOC) {
  using namespace nix;
  // previous line of code.
  if (LOC.prevLineOfCode.has_value()) {
    Out << std::endl
        << fmt("%1% %|2$5d|| %3%", Prefix, (BeginPos->line - 1),
               *LOC.prevLineOfCode);
  }

  if (LOC.errLineOfCode.has_value()) {
    // line of code containing the error.
    Out << std::endl
        << fmt("%1% %|2$5d|| %3%", Prefix, (BeginPos->line),
               *LOC.errLineOfCode);
    // error arrows for the column range.
    if (BeginPos->column > 0) {
      auto Start = BeginPos->column;
      std::string Spaces;
      for (auto I = 0; I < Start; ++I) {
        Spaces.append(" ");
      }

      std::string arrows("^");

      Out << std::endl
          << fmt("%1%      |%2%" ANSI_RED "%3%" ANSI_NORMAL, Prefix, Spaces,
                 arrows);

      if (BeginPos->line == EndPos->line) {
        Out << ANSI_RED;
        for (auto I = BeginPos->column + 1; I < EndPos->column; I++) {
          Out << (I == EndPos->column - 1 ? "^" : "~");
        }
        Out << ANSI_NORMAL;
      }
    }
  }

  // next line of code.
  if (LOC.nextLineOfCode.has_value()) {
    Out << std::endl
        << fmt("%1% %|2$5d|| %3%", Prefix, (BeginPos->line + 1),
               *LOC.nextLineOfCode);
  }
}

struct ASTDump : nixd::RecursiveASTVisitor<ASTDump> {
  nix::SymbolTable &STable;
  nix::PosTable &PTable;
  int Depth = 0;

  bool traverseExpr(const nix::Expr *E) {
    Depth++;
    if (!nixd::RecursiveASTVisitor<ASTDump>::traverseExpr(E))
      return false;
    Depth--;
    return true;
  }

  bool visitExpr(const nix::Expr *E) const {
    if (!E)
      return true;
    for (int I = 0; I < Depth; I++) {
      std::cout << " ";
    }
    std::cout << nixd::getExprName(E) << ": ";
    E->show(STable, std::cout);

    if (ShowPosition) {
      std::cout << " ";
      showPosition(*E);
    }

    std::cout << " ";
    std::cout << "\n";
    return true;
  }

  void showPosition(const nix::Expr &E) const {
    nix::PosIdx PID = E.getPos();
    if (PID != nix::noPos) {
      nix::Pos Pos = PTable[PID];
      std::cout << Pos.line << ":" << Pos.column;
    }
  }
};

int main(int argc, char *argv[]) {
  HideUnrelatedOptions(Cat);
  ParseCommandLineOptions(argc, argv, "nixd linter", nullptr,
                          "NIXD_LINTER_FLAGS");

  std::string Buffer;
  std::filesystem::path BasePath;
  nix::Pos::Origin Origin;
  if (Filename == "-") {
    BasePath = std::filesystem::current_path();
    Buffer = nix::readFile(0);
    Origin = nix::Pos::Stdin{.source = nix::make_ref<std::string>(Buffer)};
  } else {
    BasePath = std::filesystem::path{Filename.c_str()};
    BasePath = std::filesystem::absolute(BasePath);
    Buffer = nix::SourcePath(nix::CanonPath(BasePath.string())).readFile();
    Origin = nix::CanonPath(BasePath.string());
  }

  auto STable = std::make_unique<nix::SymbolTable>();
  auto PTable = std::make_unique<nix::PosTable>();
  nixd::syntax::ParseState S{*STable, *PTable};
  nixd::syntax::ParseData Data{.State = S, .Origin = Origin};
  nixd::syntax::parse(Buffer, &Data);

  nixd::EvalContext Ctx;
  nixd::Lowering Lowering{
      .STable = *STable, .PTable = *PTable, .Diags = Data.Diags, .Ctx = Ctx};
  nix::Expr *NixTree = Lowering.lower(Data.Result);

  if (DumpNixAST) {
    ASTDump D{.STable = *STable, .PTable = *PTable};
    D.traverseExpr(NixTree);
  }

  for (const auto &Diag : Data.Diags) {
    auto BeginPos = (*PTable)[Diag.Range.Begin];
    auto EndPos = (*PTable)[Diag.Range.End];
    switch (Diag.Kind) {
    case nixd::syntax::Diagnostic::Warning:
      std::cout << ANSI_WARNING "warning: " ANSI_NORMAL;
      break;
    case nixd::syntax::Diagnostic::Error:
      std::cout << ANSI_RED "error: " ANSI_NORMAL;
      break;
    }
    std::cout << Diag.Msg << "\n";
    if (BeginPos) {
      std::cout << "\n"
                << ANSI_BLUE << "at " ANSI_WARNING << BeginPos << ANSI_NORMAL
                << ":";
      if (auto Lines =
              std::shared_ptr<nix::AbstractPos>(BeginPos)->getCodeLines()) {
        std::cout << "\n";
        printCodeLines(std::cout, "", BeginPos, EndPos, *Lines);
        std::cout << "\n";
      }
    }

    for (const auto &Note : Diag.Notes) {
      auto NoteBegin = (*PTable)[Note.Range.Begin];
      auto NoteEnd = (*PTable)[Note.Range.End];
      std::cout << "\n";
      std::cout << ANSI_CYAN "note: " ANSI_NORMAL;
      std::cout << Note.Msg << "\n";

      if (NoteBegin) {
        std::cout << "\n"
                  << ANSI_BLUE << "at " ANSI_WARNING << NoteBegin << ANSI_NORMAL
                  << ":";
        if (auto Lines =
                std::shared_ptr<nix::AbstractPos>(NoteBegin)->getCodeLines()) {
          std::cout << "\n";
          printCodeLines(std::cout, "", NoteBegin, NoteEnd, *Lines);
          std::cout << "\n";
        }
      }
    }
  }
  return 0;
}
