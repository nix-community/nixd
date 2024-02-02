#include "nixt/Serialize.h"
#include "nixt/Kinds.h"

namespace nixt::serialize {

namespace {

class ASTEncoder {
  std::ostream &OS;
  const nix::SymbolTable &STable;
  const nix::PosTable &PTable;
  std::map<const nix::Expr *, std::size_t> ExprMap;

  void encodePosIdx(nix::PosIdx P) {
    // Special case for nix::noPos.
    if (P == nix::noPos) {
      encode(OS, PosInt(-1));
      return;
    }

    // Only line + column will be written to the stream, without "Origin".
    // Because Origins are too big and actually a single nix file has a unique
    // origin.
    const auto &Pos = PTable[P];
    encode(OS, Pos.line);
    encode(OS, Pos.column);
  }

  void encodeSymbol(const nix::Symbol &S) {
    encode(OS, std::string(STable[S]));
  }

  void encodeExprInt(const nix::ExprInt *E) {
    assert(E);
    ExprMap[E] = encode(OS, EncodeKind::ExprInt);
    encode(OS, E->n);
  }

  void encodeExprFloat(const nix::ExprFloat *E) {
    assert(E);
    ExprMap[E] = encode(OS, EncodeKind::ExprFloat);
    encode(OS, E->nf);
  }

  void encodeExprString(const nix::ExprString *E) {
    assert(E);
    ExprMap[E] = encode(OS, EncodeKind::ExprString);
    encode(OS, E->s);
  }

  void encodeExprPath(const nix::ExprPath *E) {
    assert(E);
    ExprMap[E] = encode(OS, EncodeKind::ExprPath);
    encode(OS, E->s);
  }

  void encodeExprVar(const nix::ExprVar *E) {
    assert(E);
    ExprMap[E] = encode(OS, EncodeKind::ExprVar);
    encodePosIdx(E->pos);
    encodeSymbol(E->name);
    encode(OS, E->fromWith);
    encode(OS, E->level);
    encode(OS, E->displ);
  }

public:
  ASTEncoder(std::ostream &OS, const nix::SymbolTable &STable,
             const nix::PosTable &PTable)
      : OS(OS), STable(STable), PTable(PTable) {
    ExprMap[nullptr] = 0;
  }

  void encodeExpr(const nix::Expr *E) {
    using namespace ek;
    if (!E)
      return;
    switch (kindOf(*E)) {
    case EK_ExprInt:
      encodeExprInt(static_cast<const nix::ExprInt *>(E));
      break;
    case EK_ExprFloat:
      encodeExprFloat(static_cast<const nix::ExprFloat *>(E));
      break;
    case EK_ExprString:
      encodeExprString(static_cast<const nix::ExprString *>(E));
      break;
    case EK_ExprPath:
      encodeExprPath(static_cast<const nix::ExprPath *>(E));
      break;
    case EK_ExprVar:
      encodeExprVar(static_cast<const nix::ExprVar *>(E));
      break;
    default:
      assert(false && "Not supported yet!");
      break;
    }
  };
};

} // namespace

std::size_t encode(std::ostream &OS, const std::string &Data) {
  // Firstly write the size of the string, because std::string may contain \0
  std::size_t Ret = encode(OS, Data.size());
  OS.write(Data.c_str(), static_cast<long>(Data.size()));
  return Ret;
}

std::size_t encode(std::ostream &OS, const nix::Pos::Origin &Origin) {
  std::size_t Ret = encode(OS, Origin.index());
  switch (Origin.index()) {
  case 0: // none_tag
  case 1: // Stdin (may break nix)
  case 2: // String (may break nix)
    break;
  case 3: // SourcePath
    encode(OS, std::get<3>(Origin).to_string());
    break;
  default:
    assert(false && "unreachable");
    break;
  }

  return Ret;
}

void encodeAST(std::ostream &OS, const nix::SymbolTable &STable,
               const nix::PosTable &PTable, const nix::Pos::Origin &Origin,
               const nix::Expr *E) {
  // Write the header, metadata stuff like a magic number and a version number.
  ASTHeader Header;
  strcpy(Header.Magic, "\x7fNixAST");
  Header.Version = 1;
  encode(OS, Header);

  encode(OS, Origin);

  ASTEncoder Encoder(OS, STable, PTable);
  Encoder.encodeExpr(E);
}

} // namespace nixt::serialize
