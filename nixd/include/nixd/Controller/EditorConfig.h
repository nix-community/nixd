#pragma once

#include "Configuration.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/strand.hpp>

#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace nixd {

struct EditorConfigSharedState;

/// Serializes workspace/configuration responses against an immutable startup
/// configuration.
class EditorConfigState {
public:
  using Executor = boost::asio::strand<boost::asio::any_io_executor>;
  using Generation = uint64_t;
  using Commit = std::function<void(Configuration)>;
  using ReportError = std::function<void(std::string)>;

private:
  std::shared_ptr<EditorConfigSharedState> Shared;

public:
  EditorConfigState(Executor Strand, Configuration Base, Commit OnCommit,
                    ReportError OnError = {});

  [[nodiscard]] std::optional<Generation> issue();
  void submit(Generation Generation,
              llvm::Expected<llvm::json::Value> Response);
  void stop();
  [[nodiscard]] bool accepting() const;
};

} // namespace nixd
