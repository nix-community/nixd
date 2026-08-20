#pragma once

#include "nixd/Support/PipedProc.h"

#include <llvm/Support/raw_ostream.h>
#include <lspserver/Connection.h>

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nixd {

struct ExecSpec {
  std::filesystem::path Executable;
  /// Complete argv, including argv[0].
  std::vector<std::string> Arguments;
  std::filesystem::path Stderr;
};

struct StreamProc {
private:
  std::unique_ptr<util::PipedProc> Proc;
  std::unique_ptr<llvm::raw_fd_ostream> Stream;

public:
  /// \brief Launch a streamed process with \p Action.
  ///
  /// The value returned by \p Action will be interpreted as process's exit
  /// value.
  StreamProc(const std::function<int()> &Action,
             std::span<const int> ChildFDs = {});

  /// Launch an executable without running application code after fork.
  explicit StreamProc(const ExecSpec &Spec);

  [[nodiscard]] llvm::raw_fd_ostream &stream() const {
    assert(Stream);
    return *Stream;
  }

  [[nodiscard]] util::PipedProc &proc() const {
    assert(Proc);
    return *Proc;
  }

  [[nodiscard]] std::unique_ptr<lspserver::InboundPort> mkIn() const;

  [[nodiscard]] std::unique_ptr<lspserver::OutboundPort> mkOut() const;
};

} // namespace nixd
