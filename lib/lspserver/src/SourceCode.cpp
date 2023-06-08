#include "lspserver/SourceCode.h"
#include "lspserver/Logger.h"
#include <llvm/Support/Errc.h>

namespace lspserver {

/// TODO: support more encodings (from clangd, using Context)
static OffsetEncoding lspEncoding() { return OffsetEncoding::UTF16; }

template <typename Callback>
static bool iterateCodepoints(llvm::StringRef U8, const Callback &CB) {
  bool LoggedInvalid = false;
  // A codepoint takes two UTF-16 code unit if it's astral (outside BMP).
  // Astral codepoints are encoded as 4 bytes in UTF-8, starting with 11110xxx.
  for (size_t I = 0; I < U8.size();) {
    unsigned char C = static_cast<unsigned char>(U8[I]);
    if (LLVM_LIKELY(!(C & 0x80))) { // ASCII character.
      if (CB(1, 1))
        return true;
      ++I;
      continue;
    }
    // This convenient property of UTF-8 holds for all non-ASCII characters.
    size_t UTF8Length = llvm::countl_one(C);
    // 0xxx is ASCII, handled above. 10xxx is a trailing byte, invalid here.
    // 11111xxx is not valid UTF-8 at all, maybe some ISO-8859-*.
    if (LLVM_UNLIKELY(UTF8Length < 2 || UTF8Length > 4)) {
      if (!LoggedInvalid) {
        elog("File has invalid UTF-8 near offset {0}: {1}", I, llvm::toHex(U8));
        LoggedInvalid = true;
      }
      // We can't give a correct result, but avoid returning something wild.
      // Pretend this is a valid ASCII byte, for lack of better options.
      // (Too late to get ISO-8859-* right, we've skipped some bytes already).
      if (CB(1, 1))
        return true;
      ++I;
      continue;
    }
    I += UTF8Length; // Skip over all trailing bytes.
    // A codepoint takes two UTF-16 code unit if it's astral (outside BMP).
    // Astral codepoints are encoded as 4 bytes in UTF-8 (11110xxx ...)
    if (CB(UTF8Length, UTF8Length == 4 ? 2 : 1))
      return true;
  }
  return false;
}

// Like most strings in clangd, the input is UTF-8 encoded.
size_t lspLength(llvm::StringRef Code) {
  size_t Count = 0;
  switch (lspEncoding()) {
  case OffsetEncoding::UTF8:
    Count = Code.size();
    break;
  case OffsetEncoding::UTF16:
    iterateCodepoints(Code, [&](int U8Len, int U16Len) {
      Count += U16Len;
      return false;
    });
    break;
  case OffsetEncoding::UTF32:
    iterateCodepoints(Code, [&](int U8Len, int U16Len) {
      ++Count;
      return false;
    });
    break;
  case OffsetEncoding::UnsupportedEncoding:
    llvm_unreachable("unsupported encoding");
  }
  return Count;
}

// Returns the byte offset into the string that is an offset of \p Units in
// the specified encoding.
// Conceptually, this converts to the encoding, truncates to CodeUnits,
// converts back to UTF-8, and returns the length in bytes.
static size_t measureUnits(llvm::StringRef U8, int Units, OffsetEncoding Enc,
                           bool &Valid) {
  Valid = Units >= 0;
  if (Units <= 0)
    return 0;
  size_t Result = 0;
  switch (Enc) {
  case OffsetEncoding::UTF8:
    Result = Units;
    break;
  case OffsetEncoding::UTF16:
    Valid = iterateCodepoints(U8, [&](int U8Len, int U16Len) {
      Result += U8Len;
      Units -= U16Len;
      return Units <= 0;
    });
    if (Units < 0) // Offset in the middle of a surrogate pair.
      Valid = false;
    break;
  case OffsetEncoding::UTF32:
    Valid = iterateCodepoints(U8, [&](int U8Len, int U16Len) {
      Result += U8Len;
      Units--;
      return Units <= 0;
    });
    break;
  case OffsetEncoding::UnsupportedEncoding:
    llvm_unreachable("unsupported encoding");
  }
  // Don't return an out-of-range index if we overran.
  if (Result > U8.size()) {
    Valid = false;
    return U8.size();
  }
  return Result;
}

llvm::Expected<size_t> positionToOffset(llvm::StringRef Code, Position P,
                                        bool AllowColumnsBeyondLineLength) {
  if (P.line < 0)
    return error(llvm::errc::invalid_argument,
                 "Line value can't be negative ({0})", P.line);
  if (P.character < 0)
    return error(llvm::errc::invalid_argument,
                 "Character value can't be negative ({0})", P.character);
  size_t StartOfLine = 0;
  for (int I = 0; I != P.line; ++I) {
    size_t NextNL = Code.find('\n', StartOfLine);
    if (NextNL == llvm::StringRef::npos)
      return error(llvm::errc::invalid_argument,
                   "Line value is out of range ({0})", P.line);
    StartOfLine = NextNL + 1;
  }
  llvm::StringRef Line =
      Code.substr(StartOfLine).take_until([](char C) { return C == '\n'; });

  // P.character may be in UTF-16, transcode if necessary.
  bool Valid;
  size_t ByteInLine = measureUnits(Line, P.character, lspEncoding(), Valid);
  if (!Valid && !AllowColumnsBeyondLineLength)
    return error(llvm::errc::invalid_argument,
                 "{0} offset {1} is invalid for line {2}", lspEncoding(),
                 P.character, P.line);
  return StartOfLine + ByteInLine;
}

Position offsetToPosition(llvm::StringRef Code, size_t Offset) {
  Offset = std::min(Code.size(), Offset);
  llvm::StringRef Before = Code.substr(0, Offset);
  int Lines = Before.count('\n');
  size_t PrevNL = Before.rfind('\n');
  size_t StartOfLine = (PrevNL == llvm::StringRef::npos) ? 0 : (PrevNL + 1);
  Position Pos;
  Pos.line = Lines;
  Pos.character = lspLength(Before.substr(StartOfLine));
  return Pos;
}

// Workaround for editors that have buggy handling of newlines at end of file.
//
// The editor is supposed to expose document contents over LSP as an exact
// string, with whitespace and newlines well-defined. But internally many
// editors treat text as an array of lines, and there can be ambiguity over
// whether the last line ends with a newline or not.
//
// This confusion can lead to incorrect edits being sent. Failing to apply them
// is catastrophic: we're desynced, LSP has no mechanism to get back in sync.
// We apply a heuristic to avoid this state.
//
// If our current view of an N-line file does *not* end in a newline, but the
// editor refers to the start of the next line (an impossible location), then
// we silently add a newline to make this valid.
// We will still validate that the rangeLength is correct, *including* the
// inferred newline.
//
// See https://github.com/neovim/neovim/issues/17085
static void inferFinalNewline(llvm::Expected<size_t> &Err,
                              std::string &Contents, const Position &Pos) {
  if (Err)
    return;
  if (!Contents.empty() && Contents.back() == '\n')
    return;
  if (Pos.character != 0)
    return;
  if (Pos.line != llvm::count(Contents, '\n') + 1)
    return;
  log("Editor sent invalid change coordinates, inferring newline at EOF");
  Contents.push_back('\n');
  consumeError(Err.takeError());
  Err = Contents.size();
}

llvm::Error applyChange(std::string &Contents,
                        const TextDocumentContentChangeEvent &Change) {
  if (!Change.range) {
    Contents = Change.text;
    return llvm::Error::success();
  }

  const Position &Start = Change.range->start;
  llvm::Expected<size_t> StartIndex = positionToOffset(Contents, Start, false);
  inferFinalNewline(StartIndex, Contents, Start);
  if (!StartIndex)
    return StartIndex.takeError();

  const Position &End = Change.range->end;
  llvm::Expected<size_t> EndIndex = positionToOffset(Contents, End, false);
  inferFinalNewline(EndIndex, Contents, End);
  if (!EndIndex)
    return EndIndex.takeError();

  if (*EndIndex < *StartIndex)
    return error(llvm::errc::invalid_argument,
                 "Range's end position ({0}) is before start position ({1})",
                 End, Start);

  // Since the range length between two LSP positions is dependent on the
  // contents of the buffer we compute the range length between the start and
  // end position ourselves and compare it to the range length of the LSP
  // message to verify the buffers of the client and server are in sync.

  // EndIndex and StartIndex are in bytes, but Change.rangeLength is in UTF-16
  // code units.
  ssize_t ComputedRangeLength =
      lspLength(Contents.substr(*StartIndex, *EndIndex - *StartIndex));

  if (Change.rangeLength && ComputedRangeLength != *Change.rangeLength)
    return error(llvm::errc::invalid_argument,
                 "Change's rangeLength ({0}) doesn't match the "
                 "computed range length ({1}).",
                 *Change.rangeLength, ComputedRangeLength);

  Contents.replace(*StartIndex, *EndIndex - *StartIndex, Change.text);

  return llvm::Error::success();
}
} // namespace lspserver
