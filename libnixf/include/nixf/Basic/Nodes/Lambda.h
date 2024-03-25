#pragma once

#include "Basic.h"

#include <map>
#include <memory>
#include <vector>

namespace nixf {

class Formal : public Node {
  std::shared_ptr<Misc> Comma;
  std::shared_ptr<Identifier> ID;
  std::shared_ptr<Expr> Default;
  std::shared_ptr<Misc> Ellipsis; // ...

public:
  Formal(LexerCursorRange Range, std::shared_ptr<Misc> Comma,
         std::shared_ptr<Identifier> ID, std::shared_ptr<Expr> Default)
      : Node(NK_Formal, Range), Comma(std::move(Comma)), ID(std::move(ID)),
        Default(std::move(Default)) {}

  Formal(LexerCursorRange Range, std::shared_ptr<Misc> Comma,
         std::shared_ptr<Misc> Ellipsis)
      : Node(NK_Formal, Range), Comma(std::move(Comma)),
        Ellipsis(std::move(Ellipsis)) {
    assert(this->Ellipsis && "Ellipsis must not be null");
  }

  [[nodiscard]] Misc &ellipsis() const {
    assert(Ellipsis && "Ellipsis must not be null");
    return *Ellipsis;
  }

  [[nodiscard]] bool isEllipsis() const { return Ellipsis != nullptr; }

  [[nodiscard]] Identifier *id() const { return ID.get(); }

  [[nodiscard]] Misc *comma() const { return Comma.get(); }

  [[nodiscard]] Expr *defaultExpr() const { return Default.get(); }

  [[nodiscard]] ChildVector children() const override {
    if (isEllipsis()) {
      return {Ellipsis.get()};
    }
    return {ID.get(), Default.get()};
  }
};

/// \brief Lambda formal arguments.
///
/// Things to check:
/// 1. Ellipsis can only occur at the end of the formals.
///        { ..., pkgs } -> { pkgs, ... }
/// 2. Ellipsis can only occur once.
///        { b, ..., a, ... } -> { a, ... }
class Formals : public Node {
  std::vector<std::shared_ptr<Formal>> Members;

  /// Deduplicated formals, useful for encoding
  std::map<std::string, const Formal *> Dedup;

public:
  using FormalVector = std::vector<std::shared_ptr<Formal>>;
  Formals(LexerCursorRange Range, FormalVector Members,
          std::map<std::string, const Formal *> Dedup)
      : Node(NK_Formals, Range), Members(std::move(Members)),
        Dedup(std::move(Dedup)) {}

  [[nodiscard]] const FormalVector &members() const { return Members; }

  /// \brief Deduplicated formals.
  const std::map<std::string, const Formal *> &dedup() { return Dedup; }

  [[nodiscard]] const std::map<std::string, const Formal *> &dedup() const {
    return Dedup;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Members.size());
    for (const auto &Member : Members) {
      Children.emplace_back(Member.get());
    }
    return Children;
  }
};

class LambdaArg : public Node {
  std::shared_ptr<Identifier> ID;
  std::shared_ptr<Formals> F;

public:
  LambdaArg(LexerCursorRange Range, std::shared_ptr<Identifier> ID,
            std::shared_ptr<Formals> F)
      : Node(NK_LambdaArg, Range), ID(std::move(ID)), F(std::move(F)) {}

  [[nodiscard]] Identifier *id() { return ID.get(); }

  [[nodiscard]] Formals *formals() const { return F.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {ID.get(), F.get()};
  }
};

class ExprLambda : public Expr {
  std::shared_ptr<LambdaArg> Arg;
  std::shared_ptr<Expr> Body;

public:
  ExprLambda(LexerCursorRange Range, std::shared_ptr<LambdaArg> Arg,
             std::shared_ptr<Expr> Body)
      : Expr(NK_ExprLambda, Range), Arg(std::move(Arg)), Body(std::move(Body)) {
  }

  [[nodiscard]] LambdaArg *arg() const { return Arg.get(); }
  [[nodiscard]] Expr *body() const { return Body.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {Arg.get(), Body.get()};
  }
};

} // namespace nixf
