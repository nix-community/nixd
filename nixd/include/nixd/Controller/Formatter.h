#pragma once

#include "nixd/Support/PipedProc.h"
#include "nixd/Support/ProcessTree.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nixd {

struct FormatterProcess {
  std::shared_ptr<ProcessTreeIdentity> Identity;
  util::AutoCloseFD Stdin;
  util::AutoCloseFD Stdout;
  util::AutoCloseFD Stderr;

  FormatterProcess(std::shared_ptr<ProcessTreeIdentity> Identity,
                   util::PipedProc Process);
  FormatterProcess(FormatterProcess &&) noexcept = default;
  FormatterProcess(const FormatterProcess &) = delete;
};

class FormatterProcessRegistry {
  mutable std::mutex Mutex;
  bool Accepting = true;
  std::vector<std::shared_ptr<ProcessTreeIdentity>> Active;
  ProcessTreeBackend Backend;

public:
  using Launcher = std::function<util::PipedProc()>;

  explicit FormatterProcessRegistry(
      ProcessTreeBackend Backend = ProcessTreeBackend::system());

  /// The gate check, child launch, and registration share one lock. Shutdown
  /// therefore sees no child or a fully registered child.
  std::optional<FormatterProcess> launch(const Launcher &Launcher);
  void deregister(const std::shared_ptr<ProcessTreeIdentity> &Identity);
  void cancel(const std::shared_ptr<ProcessTreeIdentity> &Identity) noexcept;
  void cancelAll() noexcept;

  [[nodiscard]] std::vector<pid_t> activePIDs() const;
  [[nodiscard]] bool allActiveCancellationRequested() const;
};

struct FormatterRunResult {
  std::string Stdout;
  std::string Stderr;
  int ExitStatus = -1;
  bool Cancelled = false;
};

FormatterRunResult runFormatter(FormatterProcessRegistry &Registry,
                                const std::vector<std::string> &Command,
                                const std::filesystem::path &CWD,
                                std::string_view Input);

} // namespace nixd
