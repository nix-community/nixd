import tokens


def tok_id(tok: tokens.Token):
    prefix = "tok"
    if isinstance(tok, tokens.KwToken):
        return f"{prefix}_kw_{tok.name}"
    if isinstance(tok, tokens.OpToken):
        return f"{prefix}_op_{tok.name}"
    return f"{prefix}_{tok.name}"


def generate_tokens_h() -> str:
    header = """#pragma once

#include <string_view>

namespace nixf::tok {

enum TokenKind {
"""
    for token in tokens.tokens:
        header += f"    {tok_id(token)},\n"

    header += "};\n\n"

    header += """constexpr std::string_view spelling(int Kind) {
    using namespace std::literals;
    switch (Kind) {
"""

    for token in tokens.tokens:
        header += f'        case {tok_id(token)}: return R"({token.spelling})"sv;\n'

    header += """        default: return ""sv;
    }
}
"""

    header += "} // namespace nixf::tok"

    return header


if __name__ == "__main__":
    import sys

    with open(sys.argv[1], "w") as f:
        f.write(generate_tokens_h())
