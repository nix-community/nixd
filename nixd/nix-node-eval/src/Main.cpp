
#include "EvalProvider.h"

#include <lspserver/Connection.h>

#include <nixt/InitEval.h>

#include <unistd.h>

int main() {
  nixt::initEval();
  auto In = std::make_unique<lspserver::InboundPort>(
      STDIN_FILENO, lspserver::JSONStreamStyle::Standard);

  auto Out = std::make_unique<lspserver::OutboundPort>(false);
  nixd::EvalProvider Provider(std::move(In), std::move(Out));

  Provider.run();
}
