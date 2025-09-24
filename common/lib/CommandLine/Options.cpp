#include "nixd/CommandLine/Options.h"

using namespace llvm::cl;
using namespace nixd;

OptionCategory nixd::NixdCategory("nixd library options");

opt<bool> nixd::LitTest{
    "lit-test", desc("Indicating that the server is running in lit-test mode."),
    init(false), cat(NixdCategory)};
