/// \file
/// \brief Parser interface.
///
/// libnixf parser. This is a recursive descent parser, focusing on error
/// recovery.
#pragma once

#include <memory>
#include <string_view>
#include <vector>

namespace nixf {

class Node;
class Diagnostic;

/// \brief Parse a string.
/// \param Src The string to parse.
/// \param Diags Diagnostics will be appended to this vector.
std::shared_ptr<Node> parse(std::string_view Src,
                            std::vector<Diagnostic> &Diags);

} // namespace nixf
