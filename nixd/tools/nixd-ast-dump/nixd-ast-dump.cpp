#include "nixd-config.h"

#include "nixd/Expr/Expr.h"
#include "nixd/Parser/Parser.h"
#include "nixd/Support/Position.h"

#include <nix/canon-path.hh>
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>
#include <nix/util.hh>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <iostream>
#include <memory>

namespace {

using namespace llvm::cl;

OptionCategory Misc("miscellaneous options");

const OptionCategory *Cat[] = {&Misc};

opt<std::string> Filename(Positional, desc("<input file>"), init("-"),
                          cat(Misc));

opt<bool> ShowRange("range", init(false),
                    desc("Extract range information (nixd extension)"),
                    cat(Misc));

struct ASTDump : nixd::RecursiveASTVisitor<ASTDump> {
  nix::SymbolTable &STable;
  nix::PosTable &PTable;
  std::unique_ptr<nixd::ParseData> Data;

  int Depth = 0;

  bool traverseExpr(const nix::Expr *E) {
    Depth++;
    if (!nixd::RecursiveASTVisitor<ASTDump>::traverseExpr(E))
      return false;
    Depth--;
    return true;
  }

  void showRange(const void *Ptr) const {
    try {
      auto PId = Data->locations.at(Ptr);
      auto Range = nixd::Range(nixd::RangeIdx(PId, Data->end), PTable);
      std::cout << Range.Begin.line << ":" << Range.Begin.column << " "
                << Range.End.line << ":" << Range.End.column;
    } catch (...) {
    }
  }

#define NIX_EXPR(EXPR)                                                         \
  bool visit##EXPR(const nix::EXPR *E) {                                       \
    for (int i = 0; i < Depth; i++) {                                          \
      std::cout << " ";                                                        \
    }                                                                          \
    std::cout << #EXPR << ": ";                                                \
    E->show(STable, std::cout);                                                \
    std::cout << " ";                                                          \
    if (ShowRange)                                                             \
      showRange(E);                                                            \
    std::cout << "\n";                                                         \
    return true;                                                               \
  }
#include "nixd/Expr/Nodes.inc"
#undef NIX_EXPR
};

template <class... A> void fmt(auto &OS, A... Args) {
  OS << llvm::formatv(Args...).str() << "\n";
}

} // namespace

int main(int argc, char *argv[]) {
  HideUnrelatedOptions(Cat);
  SetVersionPrinter([](llvm::raw_ostream &OS) {
    OS << "nixd-ast-dump, version: ";
#ifdef NIXD_VCS_TAG
    OS << NIXD_VCS_TAG;
#else
    OS << NIXD_VERSION;
#endif
    OS << "\n";
  });
  ParseCommandLineOptions(argc, argv, "nixd AST dumper", nullptr,
                          "NIXD_AST_DUMP_FLAGS");
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
  auto [STable, PTable, Data] =
      nixd::parse(Buffer, std::move(Origin),
                  nix::CanonPath(BasePath.remove_filename().string()));
  const auto *Root = Data->result;
  if (!Data->error.empty()) {
    fmt(std::cerr, "Note: {0} error{1} encountered:", Data->error.size(),
        Data->error.size() == 1 ? "" : "s");
    for (const auto &E : Data->error) {
      auto Err = nix::Error(E);
      std::cerr << Err.what() << "\n";
    }
  }
  ASTDump A{.STable = STable, .PTable = PTable, .Data = std::move(Data)};
  A.traverseExpr(Root);
  return 0;
}
