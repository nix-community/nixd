#pragma once

#include "Basic.h"
#include "Simple.h"

#include <map>

namespace nixf {

class AttrName : public Node {
public:
  enum AttrNameKind { ANK_ID, ANK_String, ANK_Interpolation };

private:
  const AttrNameKind Kind;
  const std::shared_ptr<Identifier> ID;
  const std::shared_ptr<ExprString> String;
  const std::shared_ptr<Interpolation> Interp;

public:
  [[nodiscard]] AttrNameKind kind() const { return Kind; }

  AttrName(std::shared_ptr<Identifier> ID, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_ID), ID(std::move(ID)) {
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
  const std::vector<std::shared_ptr<AttrName>> Names;
  const std::vector<std::shared_ptr<Dot>> Dots;

public:
  AttrPath(LexerCursorRange Range, std::vector<std::shared_ptr<AttrName>> Names,
           std::vector<std::shared_ptr<Dot>> Dots)
      : Node(NK_AttrPath, Range), Names(std::move(Names)),
        Dots(std::move(Dots)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<AttrName>> &names() const {
    return Names;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Names.size() + Dots.size());
    for (const auto &Name : Names)
      Children.push_back(Name.get());
    for (const auto &Dot : Dots)
      Children.emplace_back(Dot.get());
    return Children;
  }
};

class Binding : public Node {
  const std::shared_ptr<AttrPath> Path;
  const std::shared_ptr<Expr> Value;
  const std::shared_ptr<Misc> Eq;

public:
  Binding(LexerCursorRange Range, std::shared_ptr<AttrPath> Path,
          std::shared_ptr<Expr> Value, std::shared_ptr<Misc> Eq)
      : Node(NK_Binding, Range), Path(std::move(Path)), Value(std::move(Value)),
        Eq(std::move(Eq)) {
    assert(this->Path && "Path must not be null");
    // Value can be null, if missing in the syntax.
  }

  [[nodiscard]] const AttrPath &path() const {
    assert(Path && "Path must not be null");
    return *Path;
  }

  [[nodiscard]] const std::shared_ptr<Expr> &value() const { return Value; }

  [[nodiscard]] std::shared_ptr<Misc> eq() const { return Eq; }

  [[nodiscard]] ChildVector children() const override {
    return {Path.get(), Eq.get(), Value.get()};
  }
};

class Inherit : public Node {
  const std::vector<std::shared_ptr<AttrName>> Names;
  const std::shared_ptr<Expr> E;

public:
  Inherit(LexerCursorRange Range, std::vector<std::shared_ptr<AttrName>> Names,
          std::shared_ptr<Expr> E)
      : Node(NK_Inherit, Range), Names(std::move(Names)), E(std::move(E)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<AttrName>> &names() const {
    return Names;
  }

  [[nodiscard]] bool hasExpr() { return E != nullptr; }

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
  const std::vector<std::shared_ptr<Node>> Bindings;

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

class Attribute {
public:
  enum class AttributeKind {
    /// a = b;
    Plain,
    /// inherit a b c;
    Inherit,
    /// inherit (expr) a b c
    InheritFrom,
  };

private:
  const std::shared_ptr<Node> Key;
  const std::shared_ptr<Expr> Value;
  AttributeKind Kind;

public:
  Attribute(std::shared_ptr<Node> Key, std::shared_ptr<Expr> Value,
            AttributeKind Kind)
      : Key(std::move(Key)), Value(std::move(Value)), Kind(Kind) {
    assert(this->Key && "Key must not be null");
  }

  [[nodiscard]] Node &key() const { return *Key; }

  [[nodiscard]] Expr *value() const { return Value.get(); }

  [[nodiscard]] AttributeKind kind() const { return Kind; }

  [[nodiscard]] bool fromInherit() const {
    return Kind == AttributeKind::InheritFrom || Kind == AttributeKind::Inherit;
  }
};

/// \brief Attribute set after deduplication.
///
/// Represeting the attribute set suitable for variable lookups, evaluation.
///
/// The attrset cannot have duplicate keys, and keys will be desugared to strict
/// K-V form.
///
/// e.g. `{ a.b.c = 1 }` -> `{ a = { b = { c = 1; }; }; }`
class SemaAttrs {
private:
  // These fields are in-complete during semantic analysis.
  // So they explicitly marked as mutable
  /*mutable*/ std::map<std::string, Attribute> Static;
  /*mutable*/ std::vector<Attribute> Dynamic;

  const Misc *Recursive;

  friend class Sema;

public:
  SemaAttrs(Misc *Recursive) : Recursive(Recursive) {}
  SemaAttrs(std::map<std::string, Attribute> Static,
            std::vector<Attribute> Dynamic, Misc *Recursive)
      : Static(std::move(Static)), Dynamic(std::move(Dynamic)),
        Recursive(Recursive) {}

  /// \brief Static attributes, do not require evaluation to get the key.
  ///
  /// e.g. `{ a = 1; b = 2; }`
  [[nodiscard]] const std::map<std::string, Attribute> &staticAttrs() const {
    return Static;
  }

  /// \brief Dynamic attributes, require evaluation to get the key.
  ///
  /// e.g. `{ "${asdasda}" = "asdasd"; }`
  [[nodiscard]] const std::vector<Attribute> &dynamicAttrs() const {
    return Dynamic;
  }

  /// \brief If the attribute set is `rec`.
  [[nodiscard]] bool isRecursive() const { return Recursive; }
};

class ExprAttrs : public Expr {
  const std::shared_ptr<Binds> Body;
  const std::shared_ptr<Misc> Rec;
  SemaAttrs SA; // Let this mutable for "Sema" class only.
  friend class Sema;

public:
  ExprAttrs(LexerCursorRange Range, std::shared_ptr<Binds> Body,
            std::shared_ptr<Misc> Rec, SemaAttrs SA)
      : Expr(NK_ExprAttrs, Range), Body(std::move(Body)), Rec(std::move(Rec)),
        SA(std::move(SA)) {}

  [[nodiscard]] const Binds *binds() const { return Body.get(); }
  [[nodiscard]] const Misc *rec() const { return Rec.get(); }

  [[nodiscard]] bool isRecursive() const { return Rec != nullptr; }

  [[nodiscard]] const SemaAttrs &sema() const { return SA; }

  [[nodiscard]] ChildVector children() const override {
    return {Body.get(), Rec.get()};
  }
};

} // namespace nixf
