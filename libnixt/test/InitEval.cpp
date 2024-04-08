#include "nixt/InitEval.h"

using namespace nixt;

namespace {

__attribute__((constructor)) void init() { initEval(); }

} // namespace
