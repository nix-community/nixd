#include "nixd/Controller/EditorConfig.h"

#include <boost/asio/post.hpp>

#include <llvm/Support/FormatVariadic.h>

#include <atomic>
#include <mutex>
#include <utility>

namespace nixd {

struct EditorConfigSharedState {
  std::mutex Mutex;
  uint64_t LatestGeneration = 0;
  std::atomic<bool> Accepting{true};
  EditorConfigState::Executor Strand;
  const Configuration Base;
  EditorConfigState::Commit OnCommit;
  EditorConfigState::ReportError OnError;

  EditorConfigSharedState(EditorConfigState::Executor Strand,
                          Configuration Base,
                          EditorConfigState::Commit OnCommit,
                          EditorConfigState::ReportError OnError)
      : Strand(std::move(Strand)), Base(std::move(Base)),
        OnCommit(std::move(OnCommit)), OnError(std::move(OnError)) {}
};

namespace {

bool isCurrent(const EditorConfigSharedState &Shared, uint64_t Generation) {
  return Shared.Accepting && Shared.LatestGeneration == Generation;
}

void reportError(const std::shared_ptr<EditorConfigSharedState> &Shared,
                 uint64_t Generation, std::string Error) {
  EditorConfigState::ReportError OnError;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (isCurrent(*Shared, Generation))
      OnError = Shared->OnError;
  }
  if (OnError)
    OnError(std::move(Error));
}

void commit(const std::shared_ptr<EditorConfigSharedState> &Shared,
            uint64_t Generation, Configuration Config) {
  EditorConfigState::Commit OnCommit;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (isCurrent(*Shared, Generation))
      OnCommit = Shared->OnCommit;
  }
  if (OnCommit)
    OnCommit(std::move(Config));
}

void applyResponse(const std::shared_ptr<EditorConfigSharedState> &Shared,
                   uint64_t Generation,
                   llvm::Expected<llvm::json::Value> Response) {
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isCurrent(*Shared, Generation)) {
      if (!Response)
        llvm::consumeError(Response.takeError());
      return;
    }
  }

  if (!Response) {
    reportError(Shared, Generation,
                llvm::formatv("workspace/configuration: {0}",
                              llvm::toString(Response.takeError()))
                    .str());
    return;
  }

  auto *Array = Response->getAsArray();
  if (!Array || Array->size() != 1) {
    reportError(Shared, Generation,
                "workspace/configuration: expected exactly one array item");
    return;
  }

  const auto &First = Array->front();
  if (First.kind() == llvm::json::Value::Null) {
    commit(Shared, Generation, Shared->Base);
    return;
  }

  ConfigurationPatch Patch;
  llvm::json::Path::Root Path;
  if (!fromJSON(First, Patch, Path)) {
    reportError(Shared, Generation,
                llvm::formatv("workspace/configuration: parse error {0}",
                              llvm::toString(Path.getError()))
                    .str());
    return;
  }
  commit(Shared, Generation, overlay(Shared->Base, Patch));
}

} // namespace

EditorConfigState::EditorConfigState(Executor Strand, Configuration Base,
                                     Commit OnCommit, ReportError OnError)
    : Shared(std::make_shared<EditorConfigSharedState>(
          std::move(Strand), std::move(Base), std::move(OnCommit),
          std::move(OnError))) {}

std::optional<EditorConfigState::Generation> EditorConfigState::issue() {
  std::lock_guard Guard(Shared->Mutex);
  if (!Shared->Accepting)
    return std::nullopt;
  return ++Shared->LatestGeneration;
}

void EditorConfigState::submit(Generation Generation,
                               llvm::Expected<llvm::json::Value> Response) {
  auto State = Shared;
  boost::asio::post(State->Strand, [State = std::move(State), Generation,
                                    Response = std::move(Response)]() mutable {
    applyResponse(State, Generation, std::move(Response));
  });
}

void EditorConfigState::stop() {
  std::lock_guard Guard(Shared->Mutex);
  Shared->Accepting = false;
}

bool EditorConfigState::accepting() const { return Shared->Accepting; }

} // namespace nixd
