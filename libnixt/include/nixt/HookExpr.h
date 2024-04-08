#pragma once

#include <nix/nixexpr.hh>

#include <map>

namespace nixt {

using ValueMap = std::map<std::uintptr_t, nix::Value>;
using EnvMap = std::map<std::uintptr_t, nix::Env *>;

struct HookExprAssert : nix::ExprAssert {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprAssert(nix::ExprAssert E, ValueMap &VMap, EnvMap &EMap,
                 std::uintptr_t Handle)
      : nix::ExprAssert(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprAttrs : nix::ExprAttrs {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprAttrs(nix::ExprAttrs E, ValueMap &VMap, EnvMap &EMap,
                std::uintptr_t Handle)
      : nix::ExprAttrs(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprCall : nix::ExprCall {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprCall(nix::ExprCall E, ValueMap &VMap, EnvMap &EMap,
               std::uintptr_t Handle)
      : nix::ExprCall(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprConcatStrings : nix::ExprConcatStrings {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprConcatStrings(nix::ExprConcatStrings E, ValueMap &VMap, EnvMap &EMap,
                        std::uintptr_t Handle)
      : nix::ExprConcatStrings(std::move(E)), VMap(VMap), EMap(EMap),
        Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprFloat : nix::ExprFloat {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprFloat(nix::ExprFloat E, ValueMap &VMap, EnvMap &EMap,
                std::uintptr_t Handle)
      : nix::ExprFloat(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprIf : nix::ExprIf {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprIf(nix::ExprIf E, ValueMap &VMap, EnvMap &EMap, std::uintptr_t Handle)
      : nix::ExprIf(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprInt : nix::ExprInt {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprInt(nix::ExprInt E, ValueMap &VMap, EnvMap &EMap,
              std::uintptr_t Handle)
      : nix::ExprInt(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprLambda : nix::ExprLambda {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprLambda(nix::ExprLambda E, ValueMap &VMap, EnvMap &EMap,
                 std::uintptr_t Handle)
      : nix::ExprLambda(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprLet : nix::ExprLet {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprLet(nix::ExprLet E, ValueMap &VMap, EnvMap &EMap,
              std::uintptr_t Handle)
      : nix::ExprLet(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprList : nix::ExprList {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprList(nix::ExprList E, ValueMap &VMap, EnvMap &EMap,
               std::uintptr_t Handle)
      : nix::ExprList(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpAnd : nix::ExprOpAnd {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpAnd(nix::ExprOpAnd E, ValueMap &VMap, EnvMap &EMap,
                std::uintptr_t Handle)
      : nix::ExprOpAnd(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpConcatLists : nix::ExprOpConcatLists {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpConcatLists(nix::ExprOpConcatLists E, ValueMap &VMap, EnvMap &EMap,
                        std::uintptr_t Handle)
      : nix::ExprOpConcatLists(std::move(E)), VMap(VMap), EMap(EMap),
        Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpEq : nix::ExprOpEq {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpEq(nix::ExprOpEq E, ValueMap &VMap, EnvMap &EMap,
               std::uintptr_t Handle)
      : nix::ExprOpEq(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpHasAttr : nix::ExprOpHasAttr {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpHasAttr(nix::ExprOpHasAttr E, ValueMap &VMap, EnvMap &EMap,
                    std::uintptr_t Handle)
      : nix::ExprOpHasAttr(std::move(E)), VMap(VMap), EMap(EMap),
        Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpImpl : nix::ExprOpImpl {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpImpl(nix::ExprOpImpl E, ValueMap &VMap, EnvMap &EMap,
                 std::uintptr_t Handle)
      : nix::ExprOpImpl(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpNEq : nix::ExprOpNEq {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpNEq(nix::ExprOpNEq E, ValueMap &VMap, EnvMap &EMap,
                std::uintptr_t Handle)
      : nix::ExprOpNEq(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpNot : nix::ExprOpNot {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpNot(nix::ExprOpNot E, ValueMap &VMap, EnvMap &EMap,
                std::uintptr_t Handle)
      : nix::ExprOpNot(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpOr : nix::ExprOpOr {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpOr(nix::ExprOpOr E, ValueMap &VMap, EnvMap &EMap,
               std::uintptr_t Handle)
      : nix::ExprOpOr(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprOpUpdate : nix::ExprOpUpdate {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprOpUpdate(nix::ExprOpUpdate E, ValueMap &VMap, EnvMap &EMap,
                   std::uintptr_t Handle)
      : nix::ExprOpUpdate(std::move(E)), VMap(VMap), EMap(EMap),
        Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprPath : nix::ExprPath {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprPath(nix::ExprPath E, ValueMap &VMap, EnvMap &EMap,
               std::uintptr_t Handle)
      : nix::ExprPath(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {
    v.mkPath(&*accessor, s.c_str());
  }
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprPos : nix::ExprPos {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprPos(nix::ExprPos E, ValueMap &VMap, EnvMap &EMap,
              std::uintptr_t Handle)
      : nix::ExprPos(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprSelect : nix::ExprSelect {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprSelect(nix::ExprSelect E, ValueMap &VMap, EnvMap &EMap,
                 std::uintptr_t Handle)
      : nix::ExprSelect(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprString : nix::ExprString {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprString(nix::ExprString E, ValueMap &VMap, EnvMap &EMap,
                 std::uintptr_t Handle)
      : nix::ExprString(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {
    v.mkString(s.data());
  }
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprVar : nix::ExprVar {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprVar(nix::ExprVar E, ValueMap &VMap, EnvMap &EMap,
              std::uintptr_t Handle)
      : nix::ExprVar(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};

struct HookExprWith : nix::ExprWith {
  ValueMap &VMap;
  EnvMap &EMap;
  std::uintptr_t Handle;
  HookExprWith(nix::ExprWith E, ValueMap &VMap, EnvMap &EMap,
               std::uintptr_t Handle)
      : nix::ExprWith(std::move(E)), VMap(VMap), EMap(EMap), Handle(Handle) {}
  void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;
  std::string getName();
};
} // namespace nixt
