#pragma once

#include "AttrSetClient.h"

#include <filesystem>

namespace nixd {

void startAttrSetEval(const std::string &Name,
                      std::unique_ptr<AttrSetClientProc> &Worker,
                      const std::filesystem::path &CWD);

void startNixpkgs(std::unique_ptr<AttrSetClientProc> &NixpkgsEval,
                  const std::filesystem::path &CWD);

void startOption(const std::string &Name,
                 std::unique_ptr<AttrSetClientProc> &Worker,
                 const std::filesystem::path &CWD);

} // namespace nixd
