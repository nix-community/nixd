# Generate "DiagnosticEnum.h"
import sys

from support import lines
from diagnostic import diagnostics


output = open(sys.argv[1], "w")
_ = output.write(
    lines(
        [
            "enum DiagnosticKind {",
            *map(lambda x: f"  DK_{x['cname']},", diagnostics),
            "};",
        ]
    )
)
