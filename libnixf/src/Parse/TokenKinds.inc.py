from tokens import bin_op_tokens, keyword_tokens, tokens


def generate_token_section(section_name: str, tokens: list) -> str:
    if not tokens:
        return ""

    section = [f"#ifdef {section_name}"]
    section.extend(f"{section_name}({token.name})" for token in tokens)
    section.append(f"#endif // {section_name}\n")

    return "\n".join(section)


def generate_token_kinds_inc() -> str:
    sections = [
        generate_token_section("TOK_KEYWORD", keyword_tokens),
        generate_token_section("TOK", tokens),
        generate_token_section("TOK_BIN_OP", bin_op_tokens),
    ]

    return "\n".join(filter(None, sections)).strip()


if __name__ == "__main__":
    import sys

    with open(sys.argv[1], "w") as f:
        f.write(generate_token_kinds_inc())
