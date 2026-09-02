#!/usr/bin/env bash

set -euo pipefail

nixd="${1:?usage: cli-options.sh PATH_TO_NIXD}"
help_output="$("${nixd}" --help)"

expected=(
  "nixd library options:"
  "--config=<string>"
  "--inlay-hints"
  "--lit-test"
  "--nixos-options-expr=<string>"
  "--nixpkgs-expr=<string>"
  "--nixpkgs-worker-stderr=<string>"
  "--option-worker-stderr=<string>"
  "--semantic-tokens"
)

for item in "${expected[@]}"; do
  grep -F -- "${item}" <<< "${help_output}" > /dev/null
done

"${nixd}" --config '{}' < /dev/null
