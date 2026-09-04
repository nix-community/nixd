#pragma once

#include "AttrSetClient.h"

namespace nixd {

std::unique_ptr<AttrSetClientProc> startAttrSetEval(const std::string &Name);

std::unique_ptr<AttrSetClientProc> startNixpkgs();

std::unique_ptr<AttrSetClientProc> startOption(const std::string &Name);

} // namespace nixd
