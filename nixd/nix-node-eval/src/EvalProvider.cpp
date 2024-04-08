#include "EvalProvider.h"

#include "nixd/rpc/Protocol.h"

#include <nixt/Deserialize.h>
#include <nixt/HackCache.h>

#include <lspserver/LSPServer.h>

#include <bc/Read.h>
#include <bc/Write.h>

#include <nix/canon-path.hh>
#include <nix/eval.hh>
#include <nix/fs-input-accessor.hh>
#include <nix/input-accessor.hh>
#include <nix/nixexpr.hh>
#include <nix/store-api.hh>

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/detail/os_file_functions.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

namespace nixd {

namespace bipc = boost::interprocess;

using namespace rpc;

EvalProvider::EvalProvider(std::unique_ptr<lspserver::InboundPort> In,
                           std::unique_ptr<lspserver::OutboundPort> Out)
    : lspserver::LSPServer(std::move(In), std::move(Out)),
      State(std::unique_ptr<nix::EvalState>(
          new nix::EvalState{{}, nix::openStore("dummy://")})) {
  Registry.addMethod("exprValue", this, &EvalProvider::onExprValue);
  Registry.addNotification("registerBC", this, &EvalProvider::onRegisterBC);

  Ready = mkOutNotifiction<int>("ready");

  Ready(getpid());
}

void EvalProvider::onRegisterBC(const rpc::RegisterBCParams &Params) {
  // Path context
  auto CachePath = State->rootPath(nix::CanonPath(Params.CachePath));

  nixt::DeserializeContext Ctx = nixt::getDeserializeContext(
      *State, Params.BasePath, nix::Pos::none_tag{});

  // Extract the buffer from `Params`
  bipc::shared_memory_object Shm(bipc::open_only, Params.Shm.c_str(),
                                 bipc::read_only);

  bipc::mapped_region Region(Shm, bipc::read_only);

  std::string_view RegionView = {(char *)Region.get_address(),
                                 (char *)Region.get_address() + Params.Size};

  nix::Expr *AST = nixt::deserializeHookable(RegionView, Ctx, Pool, VMap, EMap);

  nix::Value V;
  State->eval(AST, V);

  // Inject pre-parsed AST into EvalState cache
  auto &Cache = nixt::getFileEvalCache(*State);
  Cache[CachePath] = V;
}

void EvalProvider::onExprValue(const ExprValueParams &Params,
                               lspserver::Callback<ExprValueResponse> Reply) {
  if (VMap.contains(Params.ExprID)) {
    nix::Value V = VMap[Params.ExprID];

    Reply(ExprValueResponse{
        ExprValueResponse::OK,
        static_cast<std::int64_t>(V.integer),
        ExprValueResponse::Int,
    });
    return;
  }
  Reply(ExprValueResponse{ExprValueResponse::NotEvaluated, 0});
}

} // namespace nixd
