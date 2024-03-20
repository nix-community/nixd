#pragma once

#include "Basic.h"
#include "Simple.h"

#include <map>

namespace nixf {

class AttrName : public Node {
public:
  enum AttrNameKind { ANK_ID, ANK_String, ANK_Interpolation };

private:
  AttrNameKind Kind;
  std::shared_ptr<Identifier> ID;
  std::shared_ptr<ExprString> String;
  std::shared_ptr<Interpolation> Interp;

public:
  [[nodiscard]] AttrNameKind kind() const { return Kind; }

  AttrName(std::shared_ptr<Identifier> ID, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_ID) {
    this->ID = std::move(ID);
    assert(this->ID && "ID must not be null");
  }

  AttrName(std::shared_ptr<ExprString> String)
      : Node(NK_AttrName, String->range()), Kind(ANK_String),
        String(std::move(String)) {
    assert(this->String && "String must not be null");
  }

  AttrName(std::shared_ptr<Interpolation> Interp)
      : Node(NK_AttrName, Interp->range()), Kind(ANK_Interpolation),
        Interp(std::move(Interp)) {
    assert(this->Interp && "Interpolation must not be null");
  }

  [[nodiscard]] bool isStatic() const {
    if (Kind == ANK_ID)
      return true;
    if (Kind == ANK_Interpolation)
      return false;

    assert(Kind == ANK_String);
    return string().isLiteral();
  }

  [[nodiscard]] const std::string &staticName() const {
    assert(isStatic() && "must be static");
    if (Kind == ANK_ID)
      return id()->name();
    assert(Kind == ANK_String);
    return string().literal();
  }

  [[nodiscard]] const Interpolation &interpolation() const {
    assert(Kind == ANK_Interpolation);
    assert(Interp && "Interpolation must not be null");
    return *Interp;
  }

  [[nodiscard]] const std::shared_ptr<Identifier> &id() const {
    assert(Kind == ANK_ID);
    return ID;
  }

  [[nodiscard]] std::shared_ptr<Identifier> &id() {
    assert(Kind == ANK_ID);
    return ID;
  }

  [[nodiscard]] const ExprString &string() const {
    assert(Kind == ANK_String);
    assert(String && "String must not be null");
    return *String;
  }

  [[nodiscard]] ChildVector children() const override {
    switch (Kind) {
    case ANK_ID:
      return {ID.get()};
    case ANK_String:
      return {String.get()};
    case ANK_Interpolation:
      return {Interp.get()};
    default:
      assert(false && "invalid AttrNameKind");
    }
    __builtin_unreachable();
  }
};

class AttrPath : public Node {
  std::vector<std::shared_ptr<AttrName>> Names;

public:
  AttrPath(LexerCursorRange Range, std::vector<std::shared_ptr<AttrName>> Names)
      : Node(NK_AttrPath, Range), Names(std::move(Names)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<AttrName>> &names() const {
    return Names;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Names.size());
    for (const auto &Name : Names) {
      Children.push_back(Name.get());
    }
    return Children;
  }
};

class Binding : public Node {
  std::shared_ptr<AttrPath> Path;
  std::shared_ptr<Expr> Value;

public:
  Binding(LexerCursorRange Range, std::shared_ptr<AttrPath> Path,
          std::shared_ptr<Expr> Value)
      : Node(NK_Binding, Range), Path(std::move(Path)),
        Value(std::move(Value)) {
    assert(this->Path && "Path must not be null");
    // Value can be null, if missing in the syntax.
  }

  [[nodiscard]] const AttrPath &path() const {
    assert(Path && "Path must not be null");
    return *Path;
  }

  [[nodiscard]] const std::shared_ptr<Expr> &value() const { return Value; }

  [[nodiscard]] std::shared_ptr<Expr> &value() { return Value; }

  [[nodiscard]] ChildVector children() const override {
    return {Path.get(), Value.get()};
  }
};

class Inherit : public Node {
  std::vector<std::shared_ptr<AttrName>> Names;
  std::shared_ptr<Expr> E;

public:
  Inherit(LexerCursorRange Range, std::vector<std::shared_ptr<AttrName>> Names,
          std::shared_ptr<Expr> E)
      : Node(NK_Inherit, Range), Names(std::move(Names)), E(std::move(E)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<AttrName>> &names() const {
    return Names;
  }

  [[nodiscard]] bool hasExpr() { return E != nullptr; }

  [[nodiscard]] std::shared_ptr<Expr> &expr() { return E; }

  [[nodiscard]] const std::shared_ptr<Expr> &expr() const { return E; }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Names.size() + 1);
    for (const auto &Name : Names) {
      Children.push_back(Name.get());
    }
    Children.push_back(E.get());
    return Children;
  }
};

class Binds : public Node {
  std::vector<std::shared_ptr<Node>> Bindings;

public:
  Binds(LexerCursorRange Range, std::vector<std::shared_ptr<Node>> Bindings)
      : Node(NK_Binds, Range), Bindings(std::move(Bindings)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<Node>> &bindings() const {
    return Bindings;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Bindings.size());
    for (const auto &Binding : Bindings) {
      Children.push_back(Binding.get());
    }
    return Children;
  }
};

class ExprSemaAttrs;

class ExprAttrs : public Expr {
  std::shared_ptr<Binds> Body;
  std::shared_ptr<Misc> Rec;

  std::shared_ptr<ExprSemaAttrs> Sema;

public:
  ExprAttrs(LexerCursorRange Range, std::shared_ptr<Binds> Body,
            std::shared_ptr<Misc> Rec)
      : Expr(NK_ExprAttrs, Range), Body(std::move(Body)), Rec(std::move(Rec)) {}

  [[nodiscard]] Binds *binds() const { return Body.get(); }
  [[nodiscard]] Misc *rec() const { return Rec.get(); }

  [[nodiscard]] bool isRecursive() const { return Rec != nullptr; }

  [[nodiscard]] ChildVector children() const override {
    return {Body.get(), Rec.get()};
  }

  void addSema(std::shared_ptr<ExprSemaAttrs> Sema) {
    this->Sema = std::move(Sema);
  }

  [[nodiscard]] const ExprSemaAttrs &sema() const {
    assert(Sema);
    return *Sema;
  }
};

//===----------------------------------------------------------------------===//
// Semantic nodes
//===----------------------------------------------------------------------===//

class Attr {
  std::shared_ptr<Node> Key;
  std::shared_ptr<Expr> Value;
  bool ComeFromInherit;

public:
  Attr() = default;
  Attr(std::shared_ptr<Node> Key, std::shared_ptr<Expr> Value,
       bool ComeFromInherit)
      : Key(std::move(Key)), Value(std::move(Value)),
        ComeFromInherit(ComeFromInherit) {
    assert(this->Key && "Key must not be null");
  }

  [[nodiscard]] std::shared_ptr<Node> &key() { return Key; }

  [[nodiscard]] const std::shared_ptr<Node> &key() const { return Key; }

  [[nodiscard]] std::shared_ptr<Expr> &value() { return Value; }

  [[nodiscard]] const std::shared_ptr<Expr> &value() const { return Value; }

  [[nodiscard]] bool comeFromInherit() const { return ComeFromInherit; }
};

/// \brief Attribute set after lowering.
///
/// Represeting the attribute set suitable for variable lookups, evaluation.
///
/// The attrset cannot have duplicate keys, and keys will be desugared to strict
/// K-V form.
///
/// e.g. `{ a.b.c = 1 }` -> `{ a = { b = { c = 1; }; }; }`
class ExprSemaAttrs : public Expr {
private:
  std::map<std::string, Attr> Attrs;
  std::vector<Attr> DynamicAttrs;

  bool Recursive;

public:
  ExprSemaAttrs(LexerCursorRange Range, bool Recursive)
      : Expr(NK_ExprSemaAttrs, Range), Recursive(Recursive) {}
  ExprSemaAttrs(LexerCursorRange Range, std::map<std::string, Attr> Attrs,
                std::vector<Attr> DynamicAttrs, bool Recursive)
      : Expr(NK_ExprSemaAttrs, Range), Attrs(std::move(Attrs)),
        DynamicAttrs(std::move(DynamicAttrs)), Recursive(Recursive) {}

  /// \brief Static attributes, do not require evaluation to get the key.
  ///
  /// e.g. `{ a = 1; b = 2; }`
  std::map<std::string, Attr> &staticAttrs() { return Attrs; }

  /// \brief Dynamic attributes, require evaluation to get the key.
  ///
  /// e.g. `{ "${asdasda}" = "asdasd"; }`
  std::vector<Attr> &dynamicAttrs() { return DynamicAttrs; }

  /// \brief If the attribute set is `rec`.
  [[nodiscard]] bool isRecursive() const { return Recursive; }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Vec{};
    for (const auto &[K, V] : Attrs) {
      Vec.emplace_back(V.key().get());
      Vec.emplace_back(V.value().get());
    }
    for (const auto &DA : DynamicAttrs) {
      Vec.emplace_back(DA.key().get());
      Vec.emplace_back(DA.value().get());
    }
    return Vec;
  }
};

} // namespace nixf
