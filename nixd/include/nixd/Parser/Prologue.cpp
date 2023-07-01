
#pragma once

#include "Parser.tab.h"

#include "Lexer.tab.h"

#include "Provides.h"
#include "Require.h"

#include <nix/config.hh>
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

YY_DECL;

namespace nixd {

using nix::absPath;
using nix::AttrName;
using nix::AttrPath;
using nix::Error;
using nix::ErrorInfo;
using nix::evalSettings;
using nix::experimentalFeatureSettings;
using nix::Expr;
using nix::Formal;
using nix::Formals;
using nix::getHome;
using nix::hintfmt;
using nix::noPos;
using nix::Path;
using nix::PosIdx;
using nix::PosTable;
using nix::SourcePath;
using nix::Symbol;
using nix::SymbolTable;
using nix::Xp;

using namespace nixd::nodes;

static void dupAttr(ParseData &data, const AttrPath &attrPath, const PosIdx pos,
                    const PosIdx prevPos) {
  data.error.emplace_back(
      nix::ErrorInfo{.msg = hintfmt("attribute '%1%' already defined at %2%",
                                    showAttrPath(data.state.symbols, attrPath),
                                    data.state.positions[prevPos]),
                     .errPos = data.state.positions[pos]});
}

static void dupAttr(ParseData &data, Symbol attr, const PosIdx pos,
                    const PosIdx prevPos) {
  data.error.emplace_back(nix::ErrorInfo{
      .msg = hintfmt("attribute '%1%' already defined at %2%",
                     data.state.symbols[attr], data.state.positions[prevPos]),
      .errPos = data.state.positions[pos]});
}

static void addAttr(nix::ExprAttrs *attrs, AttrPath &&attrPath, nix::Expr *e,
                    const nix::PosIdx pos, ParseData &data) {
  AttrPath::iterator i;
  // All attrpaths have at least one attr
  assert(!attrPath.empty());
  // Checking attrPath validity.
  // ===========================
  for (i = attrPath.begin(); i + 1 < attrPath.end(); i++) {
    if (i->symbol) {
      ExprAttrs::AttrDefs::iterator j = attrs->attrs.find(i->symbol);
      if (j != attrs->attrs.end()) {
        if (!j->second.inherited) {
          ExprAttrs *attrs2 = dynamic_cast<ExprAttrs *>(j->second.e);
          if (!attrs2) {

            dupAttr(data, attrPath, pos, j->second.pos);
            return;
          }
          attrs = attrs2;
        } else {

          dupAttr(data, attrPath, pos, j->second.pos);
          return;
        }
      } else {
        ExprAttrs *nested = data.ctx.record(new ExprAttrs);
        attrs->attrs[i->symbol] = ExprAttrs::AttrDef(nested, pos);
        attrs = nested;
      }
    } else {
      ExprAttrs *nested = data.ctx.record(new ExprAttrs);
      attrs->dynamicAttrs.push_back(
          ExprAttrs::DynamicAttrDef(i->expr, nested, pos));
      attrs = nested;
    }
  }
  // Expr insertion.
  // ==========================
  if (i->symbol) {
    ExprAttrs::AttrDefs::iterator j = attrs->attrs.find(i->symbol);
    if (j != attrs->attrs.end()) {
      // This attr path is already defined. However, if both
      // e and the expr pointed by the attr path are two attribute sets,
      // we want to merge them.
      // Otherwise, throw an error.
      auto ae = dynamic_cast<ExprAttrs *>(e);
      auto jAttrs = dynamic_cast<ExprAttrs *>(j->second.e);
      if (jAttrs && ae) {
        for (auto &ad : ae->attrs) {
          auto j2 = jAttrs->attrs.find(ad.first);
          if (j2 != jAttrs->attrs.end()) {
            // Attr already defined in iAttrs, error.

            dupAttr(data, ad.first, j2->second.pos, ad.second.pos);
            return;
          }
          jAttrs->attrs.emplace(ad.first, ad.second);
        }
      } else {

        dupAttr(data, attrPath, pos, j->second.pos);
        return;
      }
    } else {
      // This attr path is not defined. Let's create it.
      attrs->attrs.emplace(i->symbol, ExprAttrs::AttrDef(e, pos));
      e->setName(i->symbol);
    }
  } else {
    attrs->dynamicAttrs.push_back(ExprAttrs::DynamicAttrDef(i->expr, e, pos));
  }
}

static Formals *toFormals(ParseData &data, ParserFormals *formals,
                          PosIdx pos = nix::noPos, Symbol arg = {}) {
  std::sort(formals->formals.begin(), formals->formals.end(),
            [](const auto &a, const auto &b) {
              return std::tie(a.name, a.pos) < std::tie(b.name, b.pos);
            });

  std::optional<std::pair<Symbol, PosIdx>> duplicate;
  for (size_t i = 0; i + 1 < formals->formals.size(); i++) {
    if (formals->formals[i].name != formals->formals[i + 1].name)
      continue;
    std::pair thisDup{formals->formals[i].name, formals->formals[i + 1].pos};
    duplicate = std::min(thisDup, duplicate.value_or(thisDup));
  }
  if (duplicate)
    data.error.emplace_back(nix::ErrorInfo{
        .msg = hintfmt("duplicate formal function argument '%1%'",
                       data.state.symbols[duplicate->first]),
        .errPos = data.state.positions[duplicate->second]});

  Formals result;
  result.ellipsis = formals->ellipsis;
  result.formals = std::move(formals->formals);

  if (arg && result.has(arg))
    data.error.emplace_back(nix::ErrorInfo{
        .msg = hintfmt("duplicate formal function argument '%1%'",
                       data.state.symbols[arg]),
        .errPos = data.state.positions[pos]});

  return data.FsCtx.record(new Formals(std::move(result)));
}

static Expr *stripIndentation(
    ParseData &data, const PosIdx pos, SymbolTable &symbols,
    std::vector<std::pair<PosIdx, std::variant<Expr *, StringToken>>> &&es) {
  if (es.empty())
    return data.ctx.record(new ExprString(""));

  /* Figure out the minimum indentation.  Note that by design
     whitespace-only final lines are not taken into account.  (So
     the " " in "\n ''" is ignored, but the " " in "\n foo''" is.) */
  bool atStartOfLine = true; /* = seen only whitespace in the current line */
  size_t minIndent = 1000000;
  size_t curIndent = 0;
  for (auto &[i_pos, i] : es) {
    auto *str = std::get_if<StringToken>(&i);
    if (!str || !str->hasIndentation) {
      /* Anti-quotations and escaped characters end the current start-of-line
       * whitespace. */
      if (atStartOfLine) {
        atStartOfLine = false;
        if (curIndent < minIndent)
          minIndent = curIndent;
      }
      continue;
    }
    for (size_t j = 0; j < str->l; ++j) {
      if (atStartOfLine) {
        if (str->p[j] == ' ')
          curIndent++;
        else if (str->p[j] == '\n') {
          /* Empty line, doesn't influence minimum
             indentation. */
          curIndent = 0;
        } else {
          atStartOfLine = false;
          if (curIndent < minIndent)
            minIndent = curIndent;
        }
      } else if (str->p[j] == '\n') {
        atStartOfLine = true;
        curIndent = 0;
      }
    }
  }

  /* Strip spaces from each line. */
  auto *es2 = new std::vector<std::pair<PosIdx, Expr *>>;
  data.SPCtx.record(es2);
  atStartOfLine = true;
  size_t curDropped = 0;
  size_t n = es.size();
  auto i = es.begin();
  const auto trimExpr = [&](Expr *e) {
    atStartOfLine = false;
    curDropped = 0;
    es2->emplace_back(i->first, e);
  };
  const auto trimString = [&](const StringToken &t) {
    std::string s2;
    for (size_t j = 0; j < t.l; ++j) {
      if (atStartOfLine) {
        if (t.p[j] == ' ') {
          if (curDropped++ >= minIndent)
            s2 += t.p[j];
        } else if (t.p[j] == '\n') {
          curDropped = 0;
          s2 += t.p[j];
        } else {
          atStartOfLine = false;
          curDropped = 0;
          s2 += t.p[j];
        }
      } else {
        s2 += t.p[j];
        if (t.p[j] == '\n')
          atStartOfLine = true;
      }
    }

    /* Remove the last line if it is empty and consists only of
       spaces. */
    if (n == 1) {
      std::string::size_type p = s2.find_last_of('\n');
      if (p != std::string::npos &&
          s2.find_first_not_of(' ', p + 1) == std::string::npos)
        s2 = std::string(s2, 0, p + 1);
    }

    es2->emplace_back(i->first, data.ctx.record(new ExprString(std::move(s2))));
  };
  for (; i != es.end(); ++i, --n) {
    std::visit(nix::overloaded{trimExpr, trimString}, i->second);
  }

  /* If this is a single string, then don't do a concatenation. */
  if (es2->size() == 1 && dynamic_cast<ExprString *>((*es2)[0].second)) {
    auto *const result = (*es2)[0].second;
    return result;
  }
  return data.ctx.record(new ExprConcatStrings(pos, true, es2));
}

} // namespace nixd

using namespace nixd;

#define CUR_POS makeCurPos(*yylocp, data)

void yyerror(YYLTYPE *loc, yyscan_t scanner, ParseData *data,
             const char *error) {
  data->error.push_back(
      {.msg = hintfmt(error),
       .errPos = data->state.positions[makeCurPos(*loc, data)]});
}

template <class T> T *M(nixd::ParseData *data, T *node) {
  return data->ctx.addNode(std::unique_ptr<T>(node));
}
