#pragma once

#include "nixd/Eval/AttrSetClient.h"

namespace nixd {

void startAttrSetEval(const std::string &Name,
                      std::unique_ptr<AttrSetClientProc> &Worker);

void startNixpkgs(std::unique_ptr<AttrSetClientProc> &NixpkgsEval);

void startOption(const std::string &Name,
                 std::unique_ptr<AttrSetClientProc> &Worker);

} // namespace nixd
