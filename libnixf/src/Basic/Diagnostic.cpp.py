# Generate "DiagnosticEnum.h"
from functools import reduce
from operator import add
import sys

from diagnostic import Diagnostic, diagnostics
from support import lines, indent


def gen_parse_id() -> list[str]:
    def gen_case(d: Diagnostic) -> list[str]:
        return [
            "{" f'"{d["sname"]}", Diagnostic::DK_{d["cname"]}' "},",
        ]

    return [
        "std::optional<Diagnostic::DiagnosticKind> Diagnostic::parseKind(std::string_view SName) {",
        *map(
            indent,
            [
                "static std::unordered_map<std::string_view, nixf::Diagnostic::DiagnosticKind> DKMap {",
                *map(indent, reduce(add, map(gen_case, diagnostics))),
                "};",
                "",
                "auto It = DKMap.find(SName);",
                "if (It != DKMap.end())",
                "  return It->second;",
                "return std::nullopt;",
            ],
        ),
        "}",
    ]


def gen_message() -> list[str]:
    "Generate nixf::Diagnostic::message implementation"

    def gen_case(d: Diagnostic) -> list[str]:
        return [
            f'case DK_{d["cname"]}:',
            indent(f'return "{d["message"]}";'),
        ]

    return [
        "const char *Diagnostic::message(DiagnosticKind Kind) {",
        *map(
            indent,
            [
                "switch(Kind) {",
                *reduce(add, map(gen_case, diagnostics)),
                "}",
                "__builtin_unreachable();",
            ],
        ),
        "}",
    ]


def gen_serverity() -> list[str]:
    "Generate nixf::Diagnostic::severity implementation"

    def gen_case(d: Diagnostic) -> list[str]:
        return [
            f'case DK_{d["cname"]}:',
            indent(f'return DS_{d["severity"]};'),
        ]

    return [
        "Diagnostic::Severity Diagnostic::severity(DiagnosticKind Kind) {",
        *map(
            indent,
            [
                "switch(Kind) {",
                *reduce(add, map(gen_case, diagnostics)),
                "}",
                "__builtin_unreachable();",
            ],
        ),
        "}",
    ]


def gen_sname() -> list[str]:
    "Generate nixf::Diagnostic::sname implementation"

    def gen_case(d: Diagnostic) -> list[str]:
        return [
            f'case DK_{d["cname"]}:',
            indent(f'return "{d["sname"]}";'),
        ]

    return [
        "const char *Diagnostic::sname(DiagnosticKind Kind) {",
        *map(
            indent,
            [
                "switch(Kind) {",
                *reduce(add, map(gen_case, diagnostics)),
                "}",
                "__builtin_unreachable();",
            ],
        ),
        "}",
    ]


output = open(sys.argv[1], "w")
_ = output.write(
    lines(
        [
            '#include "nixf/Basic/Diagnostic.h"',
            "#include <unordered_map>",
            "using namespace nixf;",
            *gen_sname(),
            *gen_serverity(),
            *gen_message(),
            *gen_parse_id(),
        ]
    )
)
