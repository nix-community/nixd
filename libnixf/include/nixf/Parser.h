/// \file
/// \brief Parser interface.
///
/// libnixf parser. This is a recursive descent parser, focusing on error
/// recovery.
#pragma once

#include <memory>
#include <string_view>

namespace nixf {

class Node;
class DiagnosticEngine;

/// \brief Parse a string.
/// \param Diag Diagnostics will be written here.
std::shared_ptr<Node> parse(std::string_view Src, DiagnosticEngine &Diag);

} // namespace nixf
