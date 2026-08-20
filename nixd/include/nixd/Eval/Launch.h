#pragma once

#include "AttrSetClient.h"

#include <filesystem>
#include <functional>

namespace nixd {

void startAttrSetEval(const std::string &Name,
                      std::unique_ptr<AttrSetClientProc> &Worker,
                      const std::filesystem::path &CWD,
                      std::function<void()> OnDeath = {});

void startNixpkgs(std::unique_ptr<AttrSetClientProc> &NixpkgsEval,
                  const std::filesystem::path &CWD,
                  std::function<void()> OnDeath = {});

void startOption(const std::string &Name,
                 std::unique_ptr<AttrSetClientProc> &Worker,
                 const std::filesystem::path &CWD,
                 std::function<void()> OnDeath = {});

} // namespace nixd
