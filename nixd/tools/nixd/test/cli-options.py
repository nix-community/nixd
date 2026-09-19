import subprocess
import sys

EXPECTED_OPTIONS = [
    "nixd library options:",
    "--config=<string>",
    "--inlay-hints",
    "--lit-test",
    "--nixos-options-expr=<string>",
    "--nixpkgs-expr=<string>",
    "--nixpkgs-worker-stderr=<string>",
    "--option-worker-stderr=<string>",
    "--semantic-tokens",
]


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} PATH_TO_NIXD", file=sys.stderr)
        return 2

    nixd = sys.argv[1]
    result = subprocess.run(
        [nixd, "--help"],
        check=True,
        capture_output=True,
        text=True,
    )

    missing = [option for option in EXPECTED_OPTIONS if option not in result.stdout]
    if missing:
        print("missing command-line options:", file=sys.stderr)
        for option in missing:
            print(f"  {option}", file=sys.stderr)
        return 1

    subprocess.run(
        [nixd, "--config", "{}"],
        check=True,
        stdin=subprocess.DEVNULL,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
