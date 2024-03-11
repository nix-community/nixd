#include "nixt/ArrayRef.h"
#include "nixt/Serialize.h"

namespace nixt::serialize {
namespace {

class ASTDecoder {
  PtrPool<nix::Expr> &Pool;
  const char *Begin;
  BytesRef Cur;
  nix::PosTable &PTable;
  nix::SymbolTable &STable;
  const nix::Pos::Origin &Origin;
  std::map<std::size_t, const nix::Expr *> ExprMap;

  template <typename T> T eat() { return consume<T>(Cur); }

  [[nodiscard]] std::size_t offset() const { return Cur.Begin - Begin; }

  template <typename T, typename... ArgTs> T *create(ArgTs &&...Args) {
    auto Ptr = Pool.record(new T(std::forward<ArgTs>(Args)...));
    ExprMap[offset()] = Ptr;
    return Ptr;
  }

  nix::PosIdx decodePosIdx() {
    auto Line = eat<PosInt>();
    if (Line == PosInt(-1))
      return nix::noPos;

    auto Column = eat<PosInt>();
    return PTable.add(Origin, Line, Column);
  }

  nix::Symbol decodeSymbol() { return STable.create(eat<std::string>()); }

  nix::ExprInt *decodeExprInt() {
    return create<nix::ExprInt>(eat<nix::NixInt>());
  }

  nix::ExprFloat *decodeExprFloat() {
    return create<nix::ExprFloat>(eat<nix::NixFloat>());
  }

  nix::ExprString *decodeExprString() {
    return create<nix::ExprString>(eat<std::string>());
  }

  nix::ExprVar *decodeExprVar() {
    auto Pos = decodePosIdx();
    auto Name = decodeSymbol();
    nix::ExprVar E(Pos, Name);
    E.fromWith = eat<bool>();
    E.level = eat<nix::Level>();
    E.displ = eat<nix::Displacement>();

    return create<nix::ExprVar>(std::move(E));
  }

public:
  ASTDecoder(PtrPool<nix::Expr> &Pool, BytesRef Data, nix::PosTable &PTable,
             nix::SymbolTable &STable, const nix::Pos::Origin &Origin)
      : Pool(Pool), Begin(Data.Begin), Cur(Data), PTable(PTable),
        STable(STable), Origin(Origin) {}

  nix::Expr *decodeExpr() {
    const auto Kind = eat<EncodeKind>();
    switch (Kind) {
    case EncodeKind::ExprInt:
      return decodeExprInt();
    case EncodeKind::ExprFloat:
      return decodeExprFloat();
    case EncodeKind::ExprString:
      return decodeExprString();
    case EncodeKind::ExprVar:
      return decodeExprVar();
    default:
      assert(false && "Unknown kind");
      break;
    }
    assert(false && "unreachable");
    __builtin_unreachable();
  }

  [[nodiscard]] BytesRef getBytesRef() const { return Cur; }
};

} // namespace

std::size_t decode(BytesRef Data, std::string &Str) {
  assert(lengthof(Data) >= sizeof(std::size_t));
  std::size_t Length;
  decode(Data, Length);
  const char *Begin = Data.Begin + sizeof(std::size_t);
  Str = std::string(Begin, Length);
  return sizeof(std::size_t) + Length;
}

template <> nix::Pos::Origin consume<nix::Pos::Origin>(BytesRef &Data) {
  auto Index = consume<std::size_t>(Data);
  switch (Index) {
  case 0:
    return nix::Pos::none_tag{};
    break;
  case 1:
  case 2:
    return nix::Pos::Stdin{nix::make_ref<std::string>("")};
    break;
  default:
    assert(false && "Unknown origin");
    break;
  }
  assert(false && "unreachable");
  __builtin_unreachable();
}

nix::Expr *consumeAST(BytesRef &Data, PtrPool<nix::Expr> &Pool,
                      nix::PosTable &PTable, nix::SymbolTable &STable) {
  auto Header [[maybe_unused]] = consume<ASTHeader>(Data);
  assert(std::memcmp(Header.Magic, "\x7FNixAST\0", 8) == 0);
  assert(Header.Version == 1);
  auto Origin = consume<nix::Pos::Origin>(Data);
  ASTDecoder Decoder(Pool, Data, PTable, STable, Origin);
  nix::Expr *E = Decoder.decodeExpr();
  Data = Decoder.getBytesRef();
  return E;
}

} // namespace nixt::serialize
