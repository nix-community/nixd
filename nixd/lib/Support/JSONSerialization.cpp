#include "nixd/Support/JSONSerialization.h"

#include "lspserver/Protocol.h"

#include <llvm/Support/JSON.h>

namespace lspserver {

using llvm::json::Object;
using llvm::json::ObjectMapper;
using llvm::json::Value;

Value toJSON(const TextDocumentItem &R) {
  return Object{{"text", R.text},
                {"version", R.version},
                {"uri", R.uri},
                {"languageId", R.languageId}};
}

Value toJSON(const DidOpenTextDocumentParams &R) {
  return Object{{"textDocument", R.textDocument}};
}

bool fromJSON(const Value &Params, PublishDiagnosticsParams &R,
              llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("uri", R.uri) && O.mapOptional("version", R.version) &&
         O.map("diagnostics", R.diagnostics);
}

Value toJSON(const CompletionContext &R) {
  return Object{{"triggerCharacter", R.triggerCharacter},
                {"triggerKind", static_cast<int>(R.triggerKind)}};
}

Value toJSON(const TextDocumentPositionParams &R) {
  return Object{{"textDocument", R.textDocument}, {"position", R.position}};
}

Value toJSON(const CompletionParams &R) {
  Value Base = toJSON(static_cast<const TextDocumentPositionParams &>(R));
  Base.getAsObject()->insert({"context", R.context});
  Base.getAsObject()->insert({"limit", R.limit});
  return Base;
}

llvm::json::Value toJSON(const RenameParams &R) {
  return Object{{"newName", R.newName},
                {"position", R.position},
                {"textDocument", R.textDocument}};
}

bool fromJSON(const Value &Params, CompletionItem &R, llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("label", R.label) && O.map("kind", R.kind) &&
         O.mapOptional("detail", R.detail) &&
         O.mapOptional("documentation", R.documentation);
}

bool fromJSON(const Value &Params, CompletionList &R, llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("isIncomplete", R.isIncomplete) && O.map("items", R.items);
}

bool fromJSON(const Value &Params, MarkupContent &R, llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("value", R.value) && O.map("kind", R.kind);
}

bool fromJSON(const Value &Params, Hover &R, llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("contents", R.contents) && O.map("range", R.range);
}

bool fromJSON(const Value &Params, Location &R, llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("range", R.range) && O.map("uri", R.uri);
}

bool fromJSON(const llvm::json::Value &Params, DocumentLink &R,
              llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("range", R.range) && O.map("target", R.target);
}

bool fromJSON(const llvm::json::Value &Params, DocumentSymbol &R,
              llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("range", R.range) &&
         O.map("selectionRange", R.selectionRange) &&
         O.mapOptional("children", R.children) &&
         O.mapOptional("deprecated", R.deprecated) &&
         O.mapOptional("detail", R.detail) && O.map("name", R.name) &&
         O.map("kind", R.kind);
}

} // namespace lspserver
