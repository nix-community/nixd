
#include "EvalProvider.h"

#include <nixt/InitEval.h>

#include <unistd.h>

int main() {
  nixt::initEval();
  nixd::EvalProvider Provider(STDIN_FILENO, STDOUT_FILENO);
  return Provider.run();
}
