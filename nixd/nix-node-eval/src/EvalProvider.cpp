#include "EvalProvider.h"

#include "nixd/rpc/Protocol.h"

#include <nixt/Deserialize.h>
#include <nixt/HackCache.h>

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

using bc::readBytecode;
using rpc::readBytecode;

namespace bipc = boost::interprocess;

using namespace rpc;

EvalProvider::EvalProvider(int InboundFD, int OutboundFD)
    : rpc::Transport(InboundFD, OutboundFD),
      State(std::unique_ptr<nix::EvalState>(
          new nix::EvalState{{}, nix::openStore("dummy://")})) {}

void EvalProvider::handleInbound(const std::vector<char> &Buf) {
  std::ostringstream OS;
  rpc::RPCKind Kind;
  std::string_view Data(Buf.data(), Buf.size());
  readBytecode(Data, Kind);
  switch (Kind) {
  case rpc::RPCKind::RegisterBC: {
    rpc::RegisterBCParams Params;
    readBytecode(Data, Params);
    onRegisterBC(Params);
    break;
  }
  case rpc::RPCKind::UnregisterBC:
  case rpc::RPCKind::Log:
  case rpc::RPCKind::ExprValue: {
    rpc::ExprValueParams Params;
    readBytecode(Data, Params);
    rpc::ExprValueResponse Response = onExprValue(Params);
    sendPacket<ExprValueResponse>(Response);
    break;
  }
  }
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
  auto Cache = nixt::getFileParseCache(*State);
  Cache[CachePath] = AST;
}

ExprValueResponse EvalProvider::onExprValue(const ExprValueParams &Params) {
  if (VMap.contains(Params.ExprID)) {
    nix::Value V = VMap[Params.ExprID];

    return {
        ExprValueResponse::OK,
        static_cast<uintptr_t>(V.integer),
        ExprValueResponse::Int,
    };
  }
  return {ExprValueResponse::NotEvaluated, 0};
}

} // namespace nixd
