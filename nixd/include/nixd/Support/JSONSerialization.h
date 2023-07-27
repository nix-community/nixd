#pragma once

#include "lspserver/Protocol.h"

#include <llvm/Support/JSON.h>

#include <list>
#include <optional>
#include <thread>

// Extension to `lspserver`
namespace lspserver {

llvm::json::Value toJSON(const CompletionContext &R);

llvm::json::Value toJSON(const TextDocumentPositionParams &R);

llvm::json::Value toJSON(const CompletionParams &R);

llvm::json::Value toJSON(const RenameParams &);

llvm::json::Value toJSON(const TextDocumentItem &R);

llvm::json::Value toJSON(const DidOpenTextDocumentParams &R);

bool fromJSON(const llvm::json::Value &Params, PublishDiagnosticsParams &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, CompletionItem &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, CompletionList &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Hover &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, MarkupContent &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Location &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, DocumentLink &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, DocumentSymbol &R,
              llvm::json::Path P);

} // namespace lspserver
