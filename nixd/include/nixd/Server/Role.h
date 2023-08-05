#pragma once

namespace nixd {

enum class ServerRole {
  /// Parent process of the server
  Controller,
  /// Child process
  Evaluator,
  OptionProvider,
};

} // namespace nixd
