#pragma once

#include <bc/Read.h>

#include <cstdint>
#include <string>

namespace nixbc {

class Origin {
public:
  /// \brief Origin kind.
  ///
  /// \note Nix interpreter may read the file, or try to read source code for
  /// diagnostics reporting.
  enum OriginKind : uint8_t {
    /// None.
    OK_None,

    /// Standard input.
    OK_Stdin,

    /// \p EvalState::parseExprFromString()
    OK_String,

    /// \p EvalState::parseExprFromFile()
    OK_Path,
  };

private:
  OriginKind Kind;

protected:
  Origin(OriginKind Kind) : Kind(Kind) {}

public:
  [[nodiscard]] OriginKind kind() const { return Kind; }
};

void readBytecode(std::string_view &Data, Origin &Obj);
void writeBytecode(std::ostream &OS, const Origin &O);

class OriginPath : public Origin {
  std::string Path;

public:
  OriginPath() : Origin(OK_Path) {}

  [[nodiscard]] std::string &path() { return Path; }
  [[nodiscard]] const std::string &path() const { return Path; }
};

void readBytecode(std::string_view &Data, OriginPath &Obj);
void writeBytecode(std::ostream &OS, const OriginPath &O);

} // namespace nixbc
