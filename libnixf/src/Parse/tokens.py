from dataclasses import dataclass
from typing import List


@dataclass
class Token:
    name: str
    spelling: str


class KwToken(Token):
    def __init__(self, name):
        self.name = name
        self.spelling = name


keyword_tokens: List[Token] = [
    KwToken("if"),
    KwToken("then"),
    KwToken("else"),
    KwToken("assert"),
    KwToken("with"),
    KwToken("let"),
    KwToken("in"),
    KwToken("rec"),
    KwToken("inherit"),
    KwToken("or"),
]


class OpToken(Token):
    pass


bin_op_tokens: List[Token] = [
    OpToken("impl", "->"),
    OpToken("or", "||"),
    OpToken("and", "&&"),
    OpToken("eq", "=="),
    OpToken("neq", "!="),
    OpToken("lt", "<"),
    OpToken("gt", ">"),
    OpToken("le", "<="),
    OpToken("ge", ">="),
    OpToken("update", "//"),
    OpToken("add", "+"),
    OpToken("negate", "-"),
    OpToken("mul", "*"),
    OpToken("div", "/"),
    OpToken("concat", "++"),
    OpToken("pipe_into", "|>"),
    OpToken("pipe_from", "<|"),
]

tokens: List[Token] = [
    *keyword_tokens,
    Token("eof", "eof"),
    Token("id", "id"),
    Token("int", "int"),
    Token("float", "float"),
    Token("dquote", '"'),
    Token("string_part", "string_part"),
    Token("string_escape", "string_escape"),
    Token("quote2", "''"),
    Token("path_fragment", "path_fragment"),
    Token("spath", "<path>"),
    Token("uri", "uri"),
    Token("r_curly", "}"),
    Token("dollar_curly", "${"),
    Token("ellipsis", "..."),
    Token("comma", ","),
    Token("dot", "."),
    Token("semi_colon", ";"),
    Token("eq", "="),
    Token("l_curly", "{"),
    Token("l_paren", "("),
    Token("r_paren", ")"),
    Token("l_bracket", "["),
    Token("r_bracket", "]"),
    Token("question", "?"),
    Token("at", "@"),
    Token("colon", ":"),
    Token("unknown", "unknown"),
    Token("path_end", "path_end"),
    OpToken("not", "!"), # unary operator
    *bin_op_tokens,
]
