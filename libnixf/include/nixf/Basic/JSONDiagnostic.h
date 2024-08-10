/// \file
/// \brief Provide jsonified diagnostic, for other languages/structured output.
#pragma once

#include "Diagnostic.h"
#include "nixf/Basic/Range.h"

#include <nlohmann/json.hpp>

namespace nixf {

void to_json // NOLINT(readability-identifier-naming)
    (nlohmann::json &R, const LexerCursor &LC);

void to_json // NOLINT(readability-identifier-naming)
    (nlohmann::json &R, const LexerCursorRange &LCR);

void to_json // NOLINT(readability-identifier-naming)
    (nlohmann::json &R, const PartialDiagnostic &D);

void to_json // NOLINT(readability-identifier-naming)
    (nlohmann::json &R, const Diagnostic &D);

void to_json // NOLINT(readability-identifier-naming)
    (nlohmann::json &R, const Note &N);

void to_json // NOLINT(readability-identifier-naming)
    (nlohmann::json &R, const TextEdit &D);

void to_json // NOLINT(readability-identifier-naming)
    (nlohmann::json &R, const Fix &F);

} // namespace nixf
