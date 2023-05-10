//===--- SourceCode.h - Manipulating source code as strings -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Various code that examines C++ source code without using heavy AST machinery
// (and often not even the lexer). To be used sparingly!
//
//===----------------------------------------------------------------------===//
#pragma once

#include "Protocol.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/Error.h"
#include <optional>
#include <string>

namespace lspserver {

template <class Type> class Key {
public:
  static_assert(!std::is_reference<Type>::value,
                "Reference arguments to Key<> are not allowed");

  constexpr Key() = default;

  Key(Key const &) = delete;
  Key &operator=(Key const &) = delete;
  Key(Key &&) = delete;
  Key &operator=(Key &&) = delete;
};

// This context variable controls the behavior of functions in this file
// that convert between LSP offsets and native clang byte offsets.
// If not set, defaults to UTF-16 for backwards-compatibility.
extern Key<OffsetEncoding> kCurrentOffsetEncoding;

// Counts the number of UTF-16 code units needed to represent a string (LSP
// specifies string lengths in UTF-16 code units).
// Use of UTF-16 may be overridden by kCurrentOffsetEncoding.
size_t lspLength(llvm::StringRef Code);

/// Turn a [line, column] pair into an offset in Code.
///
/// If P.character exceeds the line length, returns the offset at end-of-line.
/// (If !AllowColumnsBeyondLineLength, then returns an error instead).
/// If the line number is out of range, returns an error.
///
/// The returned value is in the range [0, Code.size()].
llvm::Expected<size_t>
positionToOffset(llvm::StringRef Code, Position P,
                 bool AllowColumnsBeyondLineLength = true);

/// Turn an offset in Code into a [line, column] pair.
/// The offset must be in range [0, Code.size()].
Position offsetToPosition(llvm::StringRef Code, size_t Offset);

// Expand range `A` to also contain `B`.
void unionRanges(Range &A, Range B);

/// Apply an incremental update to a text document.
llvm::Error applyChange(std::string &Contents,
                        const TextDocumentContentChangeEvent &Change);

/// Collects words from the source code.
/// Unlike collectIdentifiers:
/// - also finds text in comments:
/// - splits text into words
/// - drops stopwords like "get" and "for"
llvm::StringSet<> collectWords(llvm::StringRef Content);

} // namespace lspserver
