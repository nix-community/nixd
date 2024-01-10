#pragma once

#include <memory>
#include <string_view>

namespace nixf {

class Node;
class DiagnosticEngine;
std::shared_ptr<Node> parse(std::string_view Src, DiagnosticEngine &Diag);

} // namespace nixf
