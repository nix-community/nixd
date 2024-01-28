/// \file
/// \brief AST nodes.
///
/// Declares the AST nodes used by the parser, the nodes may be used in latter
/// stages, for example semantic analysis.
/// It is expected that they may share some nodes, so they are reference
/// counted.

#pragma once

#include "nixf/Basic/Range.h"
#include "nixf/Basic/UniqueOrRaw.h"

#include <boost/container/small_vector.hpp>

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nixf {

class Node;

class HaveSyntax {
public:
  virtual ~HaveSyntax() = default;

  /// \brief The syntax node before lowering.
  ///
  /// Nullable, because nodes may be created implicitly (e.g. nested keys).
  [[nodiscard]] virtual const Node *syntax() const = 0;
};

/// \brief Evaluable nodes, after lowering.
///
/// In libnixf, we do not evaluate the code actually, instead they will
/// be serialized into byte-codes, and then evaluated by C++ nix, via IPC.
class Evaluable : public HaveSyntax {};

class Node {
public:
  enum NodeKind {
#define NODE(NAME) NK_##NAME,
#include "nixf/Basic/NodeKinds.inc"
#undef NODE
    NK_BeginExpr,
#define EXPR(NAME) NK_##NAME,
#include "nixf/Basic/NodeKinds.inc"
#undef EXPR
    NK_EndExpr,
  };

private:
  NodeKind Kind;
  LexerCursorRange Range;

protected:
  explicit Node(NodeKind Kind, LexerCursorRange Range)
      : Kind(Kind), Range(Range) {}

public:
  [[nodiscard]] NodeKind kind() const { return Kind; }
  [[nodiscard]] LexerCursorRange range() const { return Range; }
  [[nodiscard]] PositionRange positionRange() const { return Range.range(); }
  [[nodiscard]] LexerCursor lCur() const { return Range.lCur(); }
  [[nodiscard]] LexerCursor rCur() const { return Range.rCur(); }
  [[nodiscard]] static const char *name(NodeKind Kind);
  [[nodiscard]] const char *name() const { return name(Kind); }

  using ChildVector = boost::container::small_vector<Node *, 8>;

  [[nodiscard]] virtual ChildVector children() const = 0;

  virtual ~Node() = default;

  /// \brief Descendant node that contains the given range.
  [[nodiscard]] const Node *descend(PositionRange Range) const {
    if (!positionRange().contains(Range)) {
      return nullptr;
    }
    for (const auto &Child : children()) {
      if (!Child)
        continue;
      if (Child->positionRange().contains(Range)) {
        return Child->descend(Range);
      }
    }
    return this;
  }
};

class Expr : public Node, public Evaluable {
protected:
  explicit Expr(NodeKind Kind, LexerCursorRange Range) : Node(Kind, Range) {
    assert(NK_BeginExpr <= Kind && Kind <= NK_EndExpr);
  }

public:
  [[nodiscard]] const Node *syntax() const override { return this; }

  static bool classof(const Node *N) { return isExpr(N->kind()); }

  static bool isExpr(NodeKind Kind) {
    return NK_BeginExpr <= Kind && Kind <= NK_EndExpr;
  }

  /// \returns true if the expression might be evaluated to lambda.
  static bool maybeLambda(NodeKind Kind) {
    if (!isExpr(Kind))
      return false;
    switch (Kind) {
    case Node::NK_ExprInt:
    case Node::NK_ExprFloat:
    case Node::NK_ExprAttrs:
    case Node::NK_ExprString:
    case Node::NK_ExprPath:
      return false;
    default:
      return true;
    }
  }

  [[nodiscard]] bool maybeLambda() const { return maybeLambda(kind()); }
};

using NixInt = int64_t;
using NixFloat = double;

class ExprInt : public Expr {
  NixInt Value;

public:
  ExprInt(LexerCursorRange Range, NixInt Value)
      : Expr(NK_ExprInt, Range), Value(Value) {}
  [[nodiscard]] NixInt value() const { return Value; }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprFloat : public Expr {
  NixFloat Value;

public:
  ExprFloat(LexerCursorRange Range, NixFloat Value)
      : Expr(NK_ExprFloat, Range), Value(Value) {}
  [[nodiscard]] NixFloat value() const { return Value; }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class InterpolablePart {
public:
  enum InterpolablePartKind {
    SPK_Escaped,
    SPK_Interpolation,
  };

private:
  InterpolablePartKind Kind;
  std::string Escaped;
  std::unique_ptr<Expr> Interpolation;

public:
  explicit InterpolablePart(std::string Escaped)
      : Kind(SPK_Escaped), Escaped(std::move(Escaped)), Interpolation(nullptr) {
  }

  explicit InterpolablePart(std::unique_ptr<Expr> Expr)
      : Kind(SPK_Interpolation), Interpolation(std::move(Expr)) {
    assert(this->Interpolation && "interpolation must not be null");
  }

  [[nodiscard]] InterpolablePartKind kind() const { return Kind; }

  [[nodiscard]] const std::string &escaped() const {
    assert(Kind == SPK_Escaped);
    return Escaped;
  }

  [[nodiscard]] const Expr &interpolation() const {
    assert(Kind == SPK_Interpolation);
    assert(Interpolation && "interpolation must not be null");
    return *Interpolation;
  }
};

class InterpolatedParts : public Node {
  std::vector<InterpolablePart> Fragments;

public:
  InterpolatedParts(LexerCursorRange Range,
                    std::vector<InterpolablePart> Fragments);

  [[nodiscard]] const std::vector<InterpolablePart> &fragments() const {
    return Fragments;
  };

  [[nodiscard]] bool isLiteral() const {
    return Fragments.size() == 1 &&
           Fragments[0].kind() == InterpolablePart::SPK_Escaped;
  }

  [[nodiscard]] const std::string &literal() const {
    assert(isLiteral() && "must be a literal");
    return Fragments[0].escaped();
  }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprString : public Expr {
  std::unique_ptr<InterpolatedParts> Parts;

public:
  ExprString(LexerCursorRange Range, std::unique_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprString, Range), Parts(std::move(Parts)) {
    assert(this->Parts && "parts must not be null");
  }

  [[nodiscard]] const InterpolatedParts &parts() const {
    assert(Parts && "parts must not be null");
    return *Parts;
  }

  [[nodiscard]] bool isLiteral() const {
    assert(Parts && "parts must not be null");
    return Parts->isLiteral();
  }

  [[nodiscard]] const std::string &literal() const {
    assert(Parts && "parts must not be null");
    return Parts->literal();
  }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprPath : public Expr {
  std::unique_ptr<InterpolatedParts> Parts;

public:
  ExprPath(LexerCursorRange Range, std::unique_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprPath, Range), Parts(std::move(Parts)) {
    assert(this->Parts && "parts must not be null");
  }

  [[nodiscard]] const InterpolatedParts &parts() const {
    assert(Parts && "parts must not be null");
    return *Parts;
  }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

/// \brief Misc node, used for parentheses, keywords, etc.
///
/// This is used for representing nodes that only location matters.
/// Might be useful for linting.
class Misc : public Node {
public:
  Misc(LexerCursorRange Range) : Node(NK_Misc, Range) {}

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprParen : public Expr {
  std::unique_ptr<Expr> E;
  std::unique_ptr<Misc> LParen;
  std::unique_ptr<Misc> RParen;

public:
  ExprParen(LexerCursorRange Range, std::unique_ptr<Expr> E,
            std::unique_ptr<Misc> LParen, std::unique_ptr<Misc> RParen)
      : Expr(NK_ExprParen, Range), E(std::move(E)), LParen(std::move(LParen)),
        RParen(std::move(RParen)) {}

  [[nodiscard]] const Expr *expr() const { return E.get(); }
  [[nodiscard]] const Misc *lparen() const { return LParen.get(); }
  [[nodiscard]] const Misc *rparen() const { return RParen.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {E.get(), LParen.get(), RParen.get()};
  }
};

/// \brief Identifier. Variable names, attribute names, etc.
class Identifier : public Node {
  std::string Name;

public:
  Identifier(LexerCursorRange Range, std::string Name)
      : Node(NK_Identifer, Range), Name(std::move(Name)) {}
  [[nodiscard]] const std::string &name() const { return Name; }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprVar : public Expr {
  std::unique_ptr<Identifier> ID;

public:
  ExprVar(LexerCursorRange Range, std::unique_ptr<Identifier> ID)
      : Expr(NK_ExprVar, Range), ID(std::move(ID)) {
    assert(this->ID && "ID must not be null");
  }
  [[nodiscard]] const Identifier &id() const {
    assert(ID && "ID must not be null");
    return *ID;
  }

  [[nodiscard]] ChildVector children() const override { return {ID.get()}; }
};

class AttrName : public Node, public Evaluable {
public:
  enum AttrNameKind { ANK_ID, ANK_String, ANK_Interpolation };

private:
  AttrNameKind Kind;
  std::unique_ptr<Identifier> ID;
  std::unique_ptr<ExprString> String;
  std::unique_ptr<Expr> Interpolation;

public:
  [[nodiscard]] AttrNameKind kind() const { return Kind; }

  AttrName(std::unique_ptr<Identifier> ID, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_ID) {
    this->ID = std::move(ID);
    assert(this->ID && "ID must not be null");
  }

  AttrName(std::unique_ptr<ExprString> String, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_String) {
    this->String = std::move(String);
    assert(this->String && "String must not be null");
  }

  AttrName(std::unique_ptr<Expr> Interpolation, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_Interpolation) {
    this->Interpolation = std::move(Interpolation);
    assert(this->Interpolation && "Interpolation must not be null");
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

  [[nodiscard]] const Expr &interpolation() const {
    assert(Kind == ANK_Interpolation);
    assert(Interpolation && "Interpolation must not be null");
    return *Interpolation;
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
      return {Interpolation.get()};
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

class ExprSelect : public Expr {
  std::unique_ptr<Expr> E;
  std::unique_ptr<AttrPath> Path;
  std::unique_ptr<Expr> Default;

public:
  ExprSelect(LexerCursorRange Range, std::unique_ptr<Expr> E,
             std::unique_ptr<AttrPath> Path, std::unique_ptr<Expr> Default)
      : Expr(NK_ExprSelect, Range), E(std::move(E)), Path(std::move(Path)),
        Default(std::move(Default)) {
    assert(this->E && "E must not be null");
  }

  [[nodiscard]] Expr &expr() const {
    assert(E && "E must not be null");
    return *E;
  }

  [[nodiscard]] Expr *defaultExpr() const { return Default.get(); }

  [[nodiscard]] AttrPath *path() const { return Path.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {E.get(), Path.get()};
  }
};

/// A call/apply to some function.
class ExprCall : public Expr {
  std::unique_ptr<Expr> Fn;
  std::vector<std::unique_ptr<Expr>> Args;

public:
  ExprCall(LexerCursorRange Range, std::unique_ptr<Expr> Fn,
           std::vector<std::unique_ptr<Expr>> Args)
      : Expr(NK_ExprCall, Range), Fn(std::move(Fn)), Args(std::move(Args)) {
    assert(this->Fn && "Fn must not be null");
  }

  [[nodiscard]] Expr &fn() const {
    assert(Fn && "Fn must not be null");
    return *Fn;
  }
  std::vector<std::unique_ptr<Expr>> &args() { return Args; }

  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &args() const {
    return Args;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Args.size());
    for (const auto &Member : Args) {
      Children.emplace_back(Member.get());
    }
    return Children;
  }
};

class ExprList : public Expr {
  std::vector<std::unique_ptr<Expr>> Elements;

public:
  ExprList(LexerCursorRange Range, std::vector<std::unique_ptr<Expr>> Elements)
      : Expr(NK_ExprList, Range), Elements(std::move(Elements)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &elements() const {
    return Elements;
  }

  [[nodiscard]] std::vector<std::unique_ptr<Expr>> &elements() {
    return Elements;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Elements.size());
    for (const auto &Element : Elements) {
      Children.emplace_back(Element.get());
    }
    return Children;
  }
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
    assert(this->E.getRaw() && "E must not be null");
  }

  AttrBody() : Inherited(false), E(nullptr) {}

  /// \brief If the attribute is `inherit`ed.
  [[nodiscard]] bool inherited() const { return Inherited; }

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
  [[nodiscard]] Evaluable &expr() const { return *E.getRaw(); }

  [[nodiscard]] SemaAttrs *attrs() const { return E.getUnique(); }

  /// \brief The syntax node of the attribute name.
  [[nodiscard]] AttrName &name() const { return *Name; }
};

} // namespace nixf
