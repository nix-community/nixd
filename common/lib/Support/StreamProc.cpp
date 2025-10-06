#include "nixd/Support/StreamProc.h"
#include "nixd/Support/ForkPiped.h"

using namespace nixd;
using namespace util;
using namespace lspserver;

std::unique_ptr<InboundPort> StreamProc::mkIn() const {
  return std::make_unique<InboundPort>(Proc->Stdout.get(),
                                       JSONStreamStyle::Standard);
}

std::unique_ptr<OutboundPort> StreamProc::mkOut() const {
  return std::make_unique<OutboundPort>(*Stream);
}

StreamProc::StreamProc(const std::function<int()> &Action) {
  int In;
  int Out;
  int Err;

  pid_t Child = forkPiped(In, Out, Err);
  if (Child == 0)
    exit(Action());

  // Parent process.
  Proc = std::make_unique<PipedProc>(Child, In, Out, Err);
  Stream = std::make_unique<llvm::raw_fd_ostream>(In, false);
}
