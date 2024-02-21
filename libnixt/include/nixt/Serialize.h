/// \file
/// \brief Serialize nix::Expr to bytes & deserialize from bytes.

#pragma once

#include "nixt/ArrayRef.h"
#include "nixt/PtrPool.h"

#include <nixbc/Type.h>

#include <nix/nixexpr.hh>

namespace nixt {

//===----------------------------------------------------------------------===//
// Shared type definitions & constants
//===----------------------------------------------------------------------===//

enum class EncodeKind : uint32_t {
#define NIX_EXPR(EXPR) EXPR,
#include "Nodes.inc"
#undef NIX_EXPR

  // Special discriminator for nix::AttrName.
  // struct AttrName
  // {
  //     Symbol symbol;
  //     Expr * expr;
  //     AttrName(Symbol s) : symbol(s) {};
  //     AttrName(Expr * e) : expr(e) {};
  // };
  AttrNameSymbol,
};

/// \brief Header of serialized AST.
struct ASTHeader {
  char Magic[8];
  uint32_t Version;
};

//===----------------------------------------------------------------------===//
// Encoder
//===----------------------------------------------------------------------===//

/// \brief Basic primitives. Trivial data types are just written to a stream.
/// \returns The beginning offset of the data in the stream.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
std::size_t encode(std::ostream &OS, const T &Data) {
  std::size_t Ret = OS.tellp();
  OS.write(reinterpret_cast<const char *>(&Data), sizeof(Data));
  return Ret;
}

/// \brief Encode string to bytes.
std::size_t encode(std::ostream &OS, const std::string &Data);

/// \brief Encode string to bytes.
std::size_t encode(std::ostream &OS, const nix::Pos::Origin &Origin);

/// \brief Encode an AST. \p E is the root of the AST.
void encodeAST(std::ostream &OS, const nix::SymbolTable &STable,
               const nix::PosTable &PTable, const nix::Pos::Origin &Origin,
               const nix::Expr *E);

//===----------------------------------------------------------------------===//
// Decoder
//===----------------------------------------------------------------------===//

/// \brief Basic primitives. Deocde from bytes by `memcpy`.
/// \returns Size of bytes consumed.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
std::size_t decode(BytesRef Data, T &Obj) {
  assert(lengthof(Data) >= sizeof(T));
  std::memcpy(&Obj, begin(Data), sizeof(T));
  return sizeof(T);
}

/// \brief Decode string from bytes.
std::size_t decode(BytesRef Data, std::string &Str);

/// \brief Consume bytes from \p Data and construct an object of type \p T.
template <class T> T consume(BytesRef &Data) {
  T Obj;
  Data = advance(Data, decode(Data, Obj));
  return Obj;
}

nix::Expr *consumeAST(BytesRef &Data, PtrPool<nix::Expr> &Pool,
                      nix::PosTable &PTable, nix::SymbolTable &STable);

} // namespace nixt
