#include "nixd/Eval/EvalProvider.h"
#include "nixd/Protocol/Protocol.h"

#include <bc/Read.h>
#include <bc/Write.h>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/detail/os_file_functions.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <lspserver/LSPServer.h>
#include <nix/attr-path.hh>
#include <nix/store-api.hh>
#include <nixt/Deserialize.h>
#include <nixt/HackCache.h>
#include <nixt/Value.h>

#include <exception>

namespace nixd {

namespace bipc = boost::interprocess;

using namespace rpc;

EvalProvider::EvalProvider(std::unique_ptr<lspserver::InboundPort> In,
                           std::unique_ptr<lspserver::OutboundPort> Out)
    : lspserver::LSPServer(std::move(In), std::move(Out)),
      State(new nix::EvalState{{}, nix::openStore()}) {
  Registry.addMethod("exprValue", this, &EvalProvider::onExprValue);
  Registry.addMethod("registerBC", this, &EvalProvider::onRegisterBC);

  Ready = mkOutNotifiction<int>("ready");

  Ready(getpid());
}

nix::Expr *EvalProvider::fromShm(const char *ShmName, std::size_t Size,
                                 std::string_view BasePath,
                                 const nix::Pos::Origin &Origin) {
  nixt::DeserializeContext Ctx =
      nixt::getDeserializeContext(*State, BasePath, Origin);

  bipc::shared_memory_object Shm(bipc::open_only, ShmName, bipc::read_only);
  bipc::mapped_region Region(Shm, bipc::read_only);

  std::string_view RegionView = {(char *)Region.get_address(),
                                 (char *)Region.get_address() + Size};

  nix::Expr *AST = nixt::deserializeHookable(RegionView, Ctx, Pool, VMap, EMap);

  return AST;
}

void EvalProvider::onRegisterBC(
    const rpc::RegisterBCParams &Params,
    lspserver::Callback<rpc::RegisterBCResponse> Reply) {
  // Path context
  RegisterBCResponse Response;

  try {
    // Get decoding information from params.
    const auto &BasePath = Params.BasePath;
    const char *ShmName = Params.Shm.c_str();
    std::size_t Size = Params.Size;
    auto CachePath = State->rootPath(nix::CanonPath(Params.CachePath));
    nix::Pos::Origin O(CachePath);

    // Decode from shared memory, and then eval it.
    nix::Expr *AST = fromShm(ShmName, Size, BasePath, O);
    nix::Value V;
    AST->bindVars(*State, State->staticBaseEnv);
    State->eval(AST, V);

    // Inject pre-parsed AST into EvalState cache
    auto &Cache = nixt::getFileEvalCache(*State);
    Cache[CachePath] = V;
  } catch (const nix::BaseError &Err) {
    Response.ErrorMsg = Err.info().msg.str();
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
