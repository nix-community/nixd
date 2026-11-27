/// \file
/// \brief Static resolution of attributes, without evaluation.
///
/// Follows an expression to the attribute set it denotes, through `let` and
/// `rec` bindings, selections, parentheses, `let ... in` bodies and
/// `import <literal path>`. Anything else (lambda arguments, other calls,
/// `with`, dynamic names) is unresolved.

#pragma once

#include "nixd/Controller/NixTU.h"

#include "nixf/Basic/Nodes/Attrs.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nixd {

class AttrResolver {
public:
  /// A file the resolver has parsed, or was given.
  struct File {
    std::string Path;
    std::shared_ptr<const NixTU> TU;
  };

  /// An attribute, identified by its key: merged keys share one `Attribute`.
  struct Found {
    const nixf::Attribute *Attr;
    const File *In;

    [[nodiscard]] const nixf::Node &key() const { return Attr->key(); }
  };

  /// Loads \p Path, which it may change to the path of the open document that
  /// is that file.
  using Loader = std::function<std::shared_ptr<const NixTU>(std::string &Path)>;

  AttrResolver(std::string Path, std::shared_ptr<const NixTU> TU, Loader Load);

  /// The attribute that the name at \p N selects in the main file: `e.name`,
  /// `e ? name` or `inherit (e) name`.
  [[nodiscard]] std::optional<Found> attrAt(const nixf::Node &N);

  [[nodiscard]] const File &main() const { return *Main; }

private:
  struct Set {
    const nixf::ExprAttrs *Attrs;
    const File *In;
  };

  std::map<std::string, std::unique_ptr<File>> Files;
  const File *Main;
  Loader Load;
  int Depth = 0;

  const File *file(const std::string &Path);

  std::optional<Set> resolveSet(const nixf::Expr *E, const File &F);
  std::optional<Set> resolveImport(const nixf::ExprCall &Call, const File &F);
  std::optional<Found> lookup(const Set &S, const nixf::AttrName *Name);
  std::optional<Found> bindingOf(const nixf::Definition &Def, const File &F);
  std::optional<Found> keyAt(const nixf::AttrName &Name, const File &F);

  /// Resolve \p Base, then select \p Names from it one by one, calling \p Step
  /// with each name and what it selects. Stops at the first unresolved name.
  void selectEach(
      const nixf::Expr *Base,
      const std::vector<std::shared_ptr<nixf::AttrName>> &Names, const File &F,
      const std::function<void(const nixf::AttrName &, const Found &)> &Step);
};

} // namespace nixd
