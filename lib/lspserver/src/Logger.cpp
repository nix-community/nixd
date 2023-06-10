//===--- Logger.cpp - Logger interface for clangd -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lspserver/Logger.h"

#include <llvm/Support/Chrono.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <unistd.h>

#include <mutex>

namespace lspserver {

namespace {
Logger *L = nullptr;
} // namespace

LoggingSession::LoggingSession(Logger &Instance) {
  assert(!L);
  L = &Instance;
}

LoggingSession::~LoggingSession() { L = nullptr; }

void detail::logImpl(Logger::Level Level, const char *Fmt,
                     const llvm::formatv_object_base &Message) {
  if (L)
    L->log(Level, Fmt, Message);
  else {
    static std::mutex Mu;
    std::lock_guard<std::mutex> Guard(Mu);
    llvm::errs() << Message << "\n";
  }
}

const char *detail::debugType(const char *Filename) {
  if (const char *Slash = strrchr(Filename, '/'))
    return Slash + 1;
  if (const char *Backslash = strrchr(Filename, '\\'))
    return Backslash + 1;
  return Filename;
}

void StreamLogger::log(Logger::Level Level, const char *Fmt,
                       const llvm::formatv_object_base &Message) {
  using namespace boost::interprocess;
  if (Level < MinLevel)
    return;
  llvm::sys::TimePoint<> Timestamp = std::chrono::system_clock::now();
  named_mutex NamedMutex(open_or_create, "nixd.ipc.mutex.log");

  scoped_lock<named_mutex> Lock(NamedMutex);
  Logs << llvm::formatv("{0}[{1:%H:%M:%S.%L}] {2}: {3}\n", indicator(Level),
                        Timestamp, getpid(), Message);
  Logs.flush();
}

namespace {
// Like llvm::StringError but with fewer options and no gratuitous copies.
class SimpleStringError : public llvm::ErrorInfo<SimpleStringError> {
  std::error_code EC;
  std::string Message;

public:
  SimpleStringError(std::error_code EC, std::string &&Message)
      : EC(EC), Message(std::move(Message)) {}
  void log(llvm::raw_ostream &OS) const override { OS << Message; }
  std::string message() const override { return Message; }
  std::error_code convertToErrorCode() const override { return EC; }
  static char ID;
};
char SimpleStringError::ID;

} // namespace

llvm::Error detail::error(std::error_code EC, std::string &&Msg) {
  return llvm::make_error<SimpleStringError>(EC, std::move(Msg));
}

} // namespace lspserver
