#pragma once

#include "Basic.h"
#include "Simple.h"

#include <map>
#include <string>

namespace nixf {

class AttrName : public Node, public Evaluable {
public:
  enum AttrNameKind { ANK_ID, ANK_String, ANK_Interpolation };

private:
  AttrNameKind Kind;
  std::unique_ptr<Identifier> ID;
  std::unique_ptr<ExprString> String;
  std::unique_ptr<Interpolation> Interp;

public:
  [[nodiscard]] AttrNameKind kind() const { return Kind; }

  AttrName(std::unique_ptr<Identifier> ID, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_ID) {
    this->ID = std::move(ID);
    assert(this->ID && "ID must not be null");
  }

  AttrName(std::unique_ptr<ExprString> String)
      : Node(NK_AttrName, String->range()), Kind(ANK_String),
        String(std::move(String)) {
    assert(this->String && "String must not be null");
  }

  AttrName(std::unique_ptr<Interpolation> Interp)
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
      return id().name();
    assert(Kind == ANK_String);
    return string().literal();
  }

  [[nodiscard]] const Interpolation &interpolation() const {
    assert(Kind == ANK_Interpolation);
    assert(Interp && "Interpolation must not be null");
    return *Interp;
  }

  [[nodiscard]] const Identifier &id() const {
    assert(Kind == ANK_ID);
    assert(ID && "ID must not be null");
    return *ID;
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

  [[nodiscard]] const Node *syntax() const override { return this; }
};

class AttrPath : public Node {
  std::vector<std::unique_ptr<AttrName>> Names;

public:
  AttrPath(LexerCursorRange Range, std::vector<std::unique_ptr<AttrName>> Names)
      : Node(NK_AttrPath, Range), Names(std::move(Names)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<AttrName>> &names() const {
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
  std::unique_ptr<AttrPath> Path;
  std::unique_ptr<Expr> Value;

public:
  Binding(LexerCursorRange Range, std::unique_ptr<AttrPath> Path,
          std::unique_ptr<Expr> Value)
      : Node(NK_Binding, Range), Path(std::move(Path)),
        Value(std::move(Value)) {
    assert(this->Path && "Path must not be null");
    // Value can be null, if missing in the syntax.
  }

  [[nodiscard]] const AttrPath &path() const {
    assert(Path && "Path must not be null");
    return *Path;
  }
  [[nodiscard]] Expr *value() const { return Value.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {Path.get(), Value.get()};
  }
};

class Inherit : public Node {
  std::vector<std::unique_ptr<AttrName>> Names;
  std::unique_ptr<Expr> E;

public:
  Inherit(LexerCursorRange Range, std::vector<std::unique_ptr<AttrName>> Names,
          std::unique_ptr<Expr> E)
      : Node(NK_Inherit, Range), Names(std::move(Names)), E(std::move(E)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<AttrName>> &names() const {
    return Names;
  }

  [[nodiscard]] bool hasExpr() { return E != nullptr; }

  [[nodiscard]] Expr *expr() const { return E.get(); }

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
  std::vector<std::unique_ptr<Node>> Bindings;

public:
  Binds(LexerCursorRange Range, std::vector<std::unique_ptr<Node>> Bindings)
      : Node(NK_Binds, Range), Bindings(std::move(Bindings)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Node>> &bindings() const {
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

class SemaAttrs;

class ExprAttrs : public Expr {
  std::unique_ptr<Binds> Body;
  std::unique_ptr<Misc> Rec;

  std::unique_ptr<SemaAttrs> Sema;

public:
  ExprAttrs(LexerCursorRange Range, std::unique_ptr<Binds> Body,
            std::unique_ptr<Misc> Rec)
      : Expr(NK_ExprAttrs, Range), Body(std::move(Body)), Rec(std::move(Rec)) {}

  [[nodiscard]] Binds *binds() const { return Body.get(); }
  [[nodiscard]] Misc *rec() const { return Rec.get(); }

  [[nodiscard]] bool isRecursive() const { return Rec != nullptr; }

  [[nodiscard]] ChildVector children() const override {
    return {Body.get(), Rec.get()};
  }

  void addSema(std::unique_ptr<SemaAttrs> Sema) {
    this->Sema = std::move(Sema);
  }

  [[nodiscard]] const SemaAttrs &sema() const {
    assert(Sema);
    return *Sema;
  }

  [[nodiscard]] const Node *syntax() const override { return this; }
};

//===----------------------------------------------------------------------===//
// Semantic nodes
//===----------------------------------------------------------------------===//

/// \brief Semantic nodes, weak reference to syntax nodes.
///
/// These are nodes cannot implement `Evaluable` directly, and requires some
/// effort to do lowering. For example, \p ExprAttrs.
///
/// If it is easy to match `Evaluable`, then it should be implemented in syntax
/// nodes, instead of creating nodes here.
///
/// For example, \p ExprInt, \p ExprFloat.
class SemaNode : public HaveSyntax {
public:
  enum SemaKinds {
    SK_Attrs,
  };

private:
  SemaKinds Kind;
  const Node *Syntax;

protected:
  SemaNode(SemaKinds Kind, const Node *Syntax) : Kind(Kind), Syntax(Syntax) {}

public:
  /// \brief Get the kind of the node (RTTI).
  [[nodiscard]] SemaKinds kind() const { return Kind; }

  [[nodiscard]] const Node *syntax() const override { return Syntax; }
};

/// \brief Attribute set after lowering.
///
/// Represeting the attribute set suitable for variable lookups, evaluation.
///
/// The attrset cannot have duplicate keys, and keys will be desugared to strict
/// K-V form.
///
/// e.g. `{ a.b.c = 1 }` -> `{ a = { b = { c = 1; }; }; }`
class SemaAttrs : public SemaNode, public Evaluable {
public:
  class AttrBody;

  class DynamicAttr {
    UniqueOrRaw<Evaluable> Key;
    UniqueOrRaw<Evaluable> Value;

  public:
    /// \note \p Key and \p Value must not be null.
    DynamicAttr(UniqueOrRaw<Evaluable> Key, UniqueOrRaw<Evaluable> Value)
        : Key(std::move(Key)), Value(std::move(Value)) {
      assert(this->Key.getRaw() && "Key must not be null");
      assert(this->Value.getRaw() && "Value must not be null");
    }
    [[nodiscard]] Evaluable &key() const { return *Key.getRaw(); }
    [[nodiscard]] Evaluable &value() const { return *Value.getRaw(); }
  };

private:
  std::map<std::string, AttrBody> Attrs;
  std::vector<DynamicAttr> DynamicAttrs;

  bool Recursive;

public:
  explicit SemaAttrs(const Node *Syntax, bool Recursive)
      : SemaNode(SK_Attrs, Syntax), Recursive(Recursive) {}
  SemaAttrs(const Node *Syntax, std::map<std::string, AttrBody> Attrs,
            std::vector<DynamicAttr> DynamicAttrs, bool Recursive)
      : SemaNode(SK_Attrs, Syntax), Attrs(std::move(Attrs)),
        DynamicAttrs(std::move(DynamicAttrs)), Recursive(Recursive) {}

  /// \brief Static attributes, do not require evaluation to get the key.
  ///
  /// e.g. `{ a = 1; b = 2; }`
  std::map<std::string, AttrBody> &staticAttrs() { return Attrs; }

  /// \brief Dynamic attributes, require evaluation to get the key.
  ///
  /// e.g. `{ "${asdasda}" = "asdasd"; }`
  std::vector<DynamicAttr> &dynamicAttrs() { return DynamicAttrs; }

  /// \brief If the attribute set is `rec`.
  [[nodiscard]] bool isRecursive() const { return Recursive; }

  [[nodiscard]] const Node *syntax() const override {
    return SemaNode::syntax();
  }
};

class SemaAttrs::AttrBody {
  bool Inherited;

  AttrName *Name;

  UniqueOrRaw<Evaluable, SemaAttrs> E;

public:
  /// \note \p E, \p Name must not be null.
  AttrBody(bool Inherited, AttrName *Name, UniqueOrRaw<Evaluable, SemaAttrs> E)
      : Inherited(Inherited), Name(Name), E(std::move(E)) {
    assert(this->Name && "Name must not be null");
    if (!Inherited)
      assert(this->E.getRaw() && "E must not be null");
  }

  AttrBody() : Inherited(false), E(nullptr) {}

  /// \brief If the attribute is `inherit`ed.
  [[nodiscard]] bool inherited() const { return Inherited; }

  /// For `inherit` attr, the expression might be desugared into a "Select".
  /// This however might be null, e.g. `{ inherit a; }`
  [[nodiscard]] Evaluable *inheritedExpr() {
    assert(Inherited && "must be inherited");
    return E.getRaw();
  }

  /// \brief The attribute value, to be evaluated.
  ///
  /// For `inherit`,
  ///
  ///  -  inherit (expr) a -> (Select Expr Symbol("a"))
  ///  -  inherit a        -> Variable("a")
  ///
  /// For normal attr,
  ///
  ///  -  a = 1            -> ExprInt(1)
  [[nodiscard]] Evaluable &expr() const {
    assert(!Inherited && "must not be inherited");
    return *E.getRaw();
  }

  [[nodiscard]] SemaAttrs *attrs() const { return E.getUnique(); }

  /// \brief The syntax node of the attribute name.
  [[nodiscard]] AttrName &name() const { return *Name; }
};

} // namespace nixf
