#include "EvalProvider.h"

#include "nixd/rpc/Protocol.h"

#include <exception>
#include <nixt/Deserialize.h>
#include <nixt/HackCache.h>
#include <nixt/Value.h>

#include <lspserver/LSPServer.h>

#include <bc/Read.h>
#include <bc/Write.h>

#include <nix/attr-path.hh>
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
  Registry.addMethod("registerBC", this, &EvalProvider::onRegisterBC);

  Ready = mkOutNotifiction<int>("ready");

  Ready(getpid());
}

void EvalProvider::onRegisterBC(
    const rpc::RegisterBCParams &Params,
    lspserver::Callback<rpc::RegisterBCResponse> Reply) {
  // Path context
  RegisterBCResponse Response;

  try {
    auto CachePath = State->rootPath(nix::CanonPath(Params.CachePath));

    nixt::DeserializeContext Ctx =
        nixt::getDeserializeContext(*State, Params.BasePath, CachePath);

    // Extract the buffer from `Params`
    bipc::shared_memory_object Shm(bipc::open_only, Params.Shm.c_str(),
                                   bipc::read_only);

    bipc::mapped_region Region(Shm, bipc::read_only);

    std::string_view RegionView = {(char *)Region.get_address(),
                                   (char *)Region.get_address() + Params.Size};

    nix::Expr *AST =
        nixt::deserializeHookable(RegionView, Ctx, Pool, VMap, EMap);
    nix::Value V;
    AST->bindVars(*State, State->staticBaseEnv);
    State->eval(AST, V);

    // Inject pre-parsed AST into EvalState cache
    auto Cache = nixt::getFileParseCache(*State);
    Cache[CachePath] = AST;
  } catch (const nix::BaseError &Err) {
    try {
      Response.ErrorMsg = Err.info().msg.str();
    } catch (std::exception &E) {
      Response.ErrorMsg = E.what();
    }
  } catch (const std::exception &Err) {
    Response.ErrorMsg = Err.what();
  }
  Reply(std::move(Response));
}

inline lspserver::Position toLSPPos(const nix::Pos &P) {
  return {static_cast<int>(std::max(1U, P.line) - 1),
          static_cast<int>(std::max(1U, P.column) - 1)};
}

void EvalProvider::onExprValue(const ExprValueParams &Params,
                               lspserver::Callback<ExprValueResponse> Reply) {
  ExprValueResponse Response{};
  Response.ResultKind = ExprValueResponse::NotFound;
  try {
    if (!VMap.contains(Params.ExprID)) {
      Reply(std::move(Response));
      return;
    }

    nix::Value V = VMap.at(Params.ExprID);

    Response.ResultKind = ExprValueResponse::OK;

    std::ostringstream OS;
    V.print(State->symbols, OS, true, /*depth=*/2);
    Response.Description = OS.str();

    // Check if this value is actually a derivation.
    // If so, we would like to provide addtional information about it.
    if (nixt::isDerivation(*State, V)) {
      try {
        Response.DrvDesc = DerivationDescription{
            .PName = nixt::attrPathStr(*State, V, "pname"),
            .Version = nixt::attrPathStr(*State, V, "version"),
            .Position = nixt::attrPathStr(*State, V, "meta.position"),
            .Description = nixt::attrPathStr(*State, V, "meta.description"),
        };
      } catch (nix::AttrPathNotFound &Error) {
        // If some attrpath is missing for this derivation, give up.
        // FIXME: it is possible that there are some attributes missing, but we
        // discards all potentially useful values here.
        Response.DrvDesc = std::nullopt;
      }
    }
  } catch (const std::exception &What) {
    Response.ErrorMsg = What.what();
  }
  Reply(std::move(Response));
}

} // namespace nixd
