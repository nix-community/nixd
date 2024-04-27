#include "nixf/Basic/JSONDiagnostic.h"
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Range.h"

using namespace nixf;
using nlohmann::json;

void nixf::to_json(json &R, const LexerCursor &LC) {
  R = json{
      {"line", LC.line()},
      {"column", LC.column()},
      {"offset", LC.offset()},
  };
}

void nixf::to_json(json &R, const LexerCursorRange &LCR) {
  R = json{
      {"lCur", LCR.lCur()},
      {"rCur", LCR.rCur()},
  };
}

void nixf::to_json(json &R, const PartialDiagnostic &PD) {
  R = json{
      {"args", PD.args()},
      {"tag", PD.tags()},
      {"range", PD.range()},
  };
}

void nixf::to_json(json &R, const Diagnostic &D) {
  to_json(R, static_cast<const PartialDiagnostic &>(D));
  R["kind"] = D.kind();
  R["severity"] = Diagnostic::severity(D.kind());
  R["message"] = D.message();
  R["sname"] = D.sname();
  R["notes"] = D.notes();
  R["fixes"] = D.fixes();
}

void nixf::to_json(json &R, const Note &N) {
  to_json(R, static_cast<const PartialDiagnostic &>(N));
  R["kind"] = N.kind();
  R["sname"] = N.sname();
  R["message"] = N.message();
}

void nixf::to_json(json &R, const Fix &F) {
  R = json{
      {"edits", F.edits()},
      {"message", F.message()},
  };
}

void nixf::to_json(json &R, const TextEdit &D) {
  R = json{
      {"range", D.oldRange()},
      {"newText", D.newText()},
  };
}
