//===--- DraftStore.h - File contents container -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include "Path.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/VirtualFileSystem.h"
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace lspserver {

/// A thread-safe container for files opened in a workspace, addressed by
/// filenames. The contents are owned by the DraftStore.
/// Each time a draft is updated, it is assigned a version. This can be
/// specified by the caller or incremented from the previous version.
class DraftStore {
public:
  struct Draft {
    std::shared_ptr<const std::string> Contents;
    std::string Version;
  };

  /// \return Contents of the stored document.
  /// For untracked files, a std::nullopt is returned.
  std::optional<Draft> getDraft(PathRef File) const;

  /// \return List of names of the drafts in this store.
  std::vector<Path> getActiveFiles() const;

  /// Replace contents of the draft for \p File with \p Contents.
  /// If version is empty, one will be automatically assigned.
  /// Returns the version.
  std::string addDraft(PathRef File, llvm::StringRef Version,
                       llvm::StringRef Contents);

  /// Remove the draft from the store.
  void removeDraft(PathRef File);

  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> asVFS() const;

  static std::optional<int64_t> decodeVersion(llvm::StringRef Encoded);

private:
  struct DraftAndTime {
    Draft D;
    std::time_t MTime;
  };
  mutable std::mutex Mutex;
  llvm::StringMap<DraftAndTime> Drafts;
};

} // namespace lspserver
