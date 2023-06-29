// \$\$ = (.*);
// { $$$$ = $1;  data->locations[$$$$] = CUR_POS; }
%glr-parser
%define api.pure
%locations
%define parse.error verbose
%defines
/* %no-lines */
%parse-param { void * scanner }
%parse-param { nixd::ParseData * data }
%lex-param { void * scanner }
%lex-param { nixd::ParseData * data }
%expect 1
%expect-rr 1

%code requires {
#include "nixd/Parser/Require.h"
}

%{
#include "nixd/Parser/Prologue.h"

template<class T>
T* M(nixd::ParseData *data, T *node) {
  return data->ctx.addNode(std::unique_ptr<T>(node));
}

%}

%union {
  // !!! We're probably leaking stuff here.
  nix::Expr * e;
  nix::ExprList * list;
  nix::ExprAttrs * attrs;
  nixd::ParserFormals * formals;
  nix::Formal * formal;
  nix::NixInt n;
  nix::NixFloat nf;
  StringToken id; // !!! -> Symbol
  StringToken path;
  StringToken uri;
  StringToken str;
  std::vector<nix::AttrName> * attrNames;
  std::vector<std::pair<nix::PosIdx, nix::Expr *>> * string_parts;
  std::vector<std::pair<nix::PosIdx, std::variant<nix::Expr *, StringToken>>> * ind_string_parts;
}

%type <e> start expr expr_function expr_if expr_op
%type <e> expr_select expr_simple expr_app
%type <list> expr_list
%type <attrs> binds
%type <formals> formals
%type <formal> formal
%type <attrNames> attrs attrpath
%type <string_parts> string_parts_interpolated
%type <ind_string_parts> ind_string_parts
%type <e> path_start string_parts string_attr
%type <id> attr
%token <id> ID ATTRPATH
%token <str> STR IND_STR
%token <n> INT
%token <nf> FLOAT
%token <path> PATH HPATH SPATH PATH_END
%token <uri> URI
%token IF THEN ELSE ASSERT WITH LET IN REC INHERIT EQ NEQ AND OR IMPL OR_KW
%token DOLLAR_CURLY /* == ${ */
%token IND_STRING_OPEN IND_STRING_CLOSE
%token ELLIPSIS

%right IMPL
%left OR
%left AND
%nonassoc EQ NEQ
%nonassoc '<' '>' LEQ GEQ
%right UPDATE
%left NOT
%left '+' '-'
%left '*' '/'
%right CONCAT
%nonassoc '?'
%nonassoc NEGATE

%%

start: expr { data->result = $1; };

expr: expr_function;

expr_function
  : ID ':' expr_function {
    $$ = M(data, new ExprLambda(CUR_POS, data->state.symbols.create($1), 0, $3));
    data->locations[$$] = CUR_POS;
  }
  | '{' formals '}' ':' expr_function {
    $$ = M(data, new ExprLambda(CUR_POS, toFormals(*data, $2), $5));
    data->locations[$$] = CUR_POS;
  }
  | '{' formals '}' '@' ID ':' expr_function {
    auto arg = data->state.symbols.create($5);
    $$ = M(data, new ExprLambda(CUR_POS, arg, toFormals(*data, $2, CUR_POS, arg), $7));
    data->locations[$$] = CUR_POS;
  }
  | ID '@' '{' formals '}' ':' expr_function {
    auto arg = data->state.symbols.create($1);
    $$ = M(data, new ExprLambda(CUR_POS, arg, toFormals(*data, $4, CUR_POS, arg), $7));
     data->locations[$$] = CUR_POS;
  }
  | ASSERT expr ';' expr_function {
    $$ = M(data, new ExprAssert(CUR_POS, $2, $4));
    data->locations[$$] = CUR_POS;
  }
  | WITH expr ';' expr_function {
    $$ = M(data, new ExprWith(CUR_POS, $2, $4));
    data->locations[$$] = CUR_POS;
  }
  | LET binds IN expr_function
    { if (!$2->dynamicAttrs.empty())
        data->error.emplace_back(nix::ErrorInfo{
            .msg = hintfmt("dynamic attributes not allowed in let"),
            .errPos = data->state.positions[CUR_POS]
        });
      { $$ = M(data, new ExprLet($2, $4));  data->locations[$$] = CUR_POS; }
    }
  | expr_if
  ;

expr_if
  : IF expr THEN expr ELSE expr { { $$ = M(data, new ExprIf(CUR_POS, $2, $4, $6));  data->locations[$$] = CUR_POS; } }
  | expr_op
  ;

expr_op
  : '!' expr_op %prec NOT { { $$ = M(data, new ExprOpNot($2));  data->locations[$$] = CUR_POS; } }
  | '-' expr_op %prec NEGATE {
    auto *ev = M(data, new ExprVar(data->state.symbols.create("__sub")));
    auto *e0 = M(data, new ExprInt(0));
    $$ = M(data, new ExprCall(CUR_POS, ev, {e0, $2}));
    data->locations[$$] = CUR_POS;
  }
  | expr_op EQ expr_op {
    $$ = M(data, new ExprOpEq($1, $3));
    data->locations[$$] = CUR_POS;
  }
  | expr_op NEQ expr_op {
    $$ = M(data, new ExprOpNEq($1, $3));
    data->locations[$$] = CUR_POS;
  }
  | expr_op '<' expr_op {
    auto *ev = M(data, new ExprVar(data->state.symbols.create("__lessThan")));
    $$ = M(data, new ExprCall(makeCurPos(@2, data), ev, {$1, $3}));
    data->locations[$$] = CUR_POS;
  }
  | expr_op LEQ expr_op {
    auto *ev = M(data, new ExprVar(data->state.symbols.create("__lessThan")));
    $$ = M(data, new ExprOpNot(M(data, new ExprCall(makeCurPos(@2, data), ev, {$3, $1}))));
    data->locations[$$] = CUR_POS;
  }
  | expr_op '>' expr_op {
    auto *ev = M(data, new ExprVar(data->state.symbols.create("__lessThan")));
    $$ = M(data, new ExprCall(makeCurPos(@2, data), ev, {$3, $1}));
    data->locations[$$] = CUR_POS;
  }
  | expr_op GEQ expr_op {
    auto *ev = M(data, new ExprVar(data->state.symbols.create("__lessThan")));
    $$ = M(data, new ExprOpNot(M(data, new ExprCall(makeCurPos(@2, data), ev, {$1, $3}))));
    data->locations[$$] = CUR_POS;
  }
  | expr_op AND expr_op {
    $$ = M(data, new ExprOpAnd(makeCurPos(@2, data), $1, $3));  data->locations[$$] = CUR_POS;
  }
  | expr_op OR expr_op {
    $$ = M(data, new ExprOpOr(makeCurPos(@2, data), $1, $3));  data->locations[$$] = CUR_POS;
  }
  | expr_op IMPL expr_op { { $$ = M(data, new ExprOpImpl(makeCurPos(@2, data), $1, $3));  data->locations[$$] = CUR_POS; } }
  | expr_op UPDATE expr_op { { $$ = M(data, new ExprOpUpdate(makeCurPos(@2, data), $1, $3));  data->locations[$$] = CUR_POS; } }
  | expr_op '?' attrpath { { $$ = M(data, new ExprOpHasAttr($1, std::move(*$3)));   data->locations[$$] = CUR_POS; } }
  | expr_op '+' expr_op {
    auto *Sp = new std::vector<std::pair<PosIdx, Expr *> >({{makeCurPos(@1, data), $1}, {makeCurPos(@3, data), $3}});
    data->SPCtx.record(Sp);
    $$ = M(data, new ExprConcatStrings(makeCurPos(@2, data), false, Sp));
    data->locations[$$] = CUR_POS;
  }
  | expr_op '-' expr_op {
     $$ = M(data, new ExprCall(makeCurPos(@2, data), M(data, new ExprVar(data->state.symbols.create("__sub"))), {$1, $3}));
     data->locations[$$] = CUR_POS;
  }
  | expr_op '*' expr_op { { $$ = M(data, new ExprCall(makeCurPos(@2, data), M(data, new ExprVar(data->state.symbols.create("__mul"))), {$1, $3}));  data->locations[$$] = CUR_POS; } }
  | expr_op '/' expr_op { { $$ = M(data, new ExprCall(makeCurPos(@2, data), M(data, new ExprVar(data->state.symbols.create("__div"))), {$1, $3}));  data->locations[$$] = CUR_POS; } }
  | expr_op CONCAT expr_op { { $$ = M(data, new ExprOpConcatLists(makeCurPos(@2, data), $1, $3));  data->locations[$$] = CUR_POS; } }
  | expr_app
  ;

expr_app
  : expr_app expr_select {
      if (auto e2 = dynamic_cast<ExprCall *>($1)) {
          e2->args.push_back($2);
          { $$ = $1;  data->locations[$$] = CUR_POS; }
      } else
          { $$ = M(data, new ExprCall(CUR_POS, $1, {$2}));  data->locations[$$] = CUR_POS; }
  }
  | expr_select
  ;

expr_select
  : expr_simple '.' attrpath
    { { $$ = M(data, new ExprSelect(CUR_POS, $1, std::move(*$3), nullptr));   data->locations[$$] = CUR_POS; } }
  | expr_simple '.' attrpath OR_KW expr_select
    { { $$ = M(data, new ExprSelect(CUR_POS, $1, std::move(*$3), $5));   data->locations[$$] = CUR_POS; } }
  | /* Backwards compatibility: because Nixpkgs has a rarely used
       function named ‘or’, allow stuff like ‘map or [...]’. */
    expr_simple OR_KW
    { { $$ = M(data, new ExprCall(CUR_POS, $1, {M(data, new ExprVar(CUR_POS, data->state.symbols.create("or")))}));  data->locations[$$] = CUR_POS; } }
  | expr_simple
  ;

expr_simple
  : ID {
      std::string_view s = "__curPos";
      if ($1.l == s.size() && strncmp($1.p, s.data(), s.size()) == 0)
          { $$ = M(data, new ExprPos(CUR_POS));  data->locations[$$] = CUR_POS; }
      else
          { $$ = M(data, new ExprVar(CUR_POS, data->state.symbols.create($1)));  data->locations[$$] = CUR_POS; }
  }
  | INT { { $$ = M(data, new ExprInt($1));  data->locations[$$] = CUR_POS; } }
  | FLOAT { { $$ = M(data, new ExprFloat($1));  data->locations[$$] = CUR_POS; } }
  | '"' string_parts '"' { { $$ = $2;  data->locations[$$] = CUR_POS; } }
  | IND_STRING_OPEN ind_string_parts IND_STRING_CLOSE {
      { $$ = stripIndentation(*data, CUR_POS, data->state.symbols, std::move(*$2));  data->locations[$$] = CUR_POS; }

  }
  | path_start PATH_END
  | path_start string_parts_interpolated PATH_END {
      $2->insert($2->begin(), {makeCurPos(@1, data), $1});
      { $$ = M(data, new ExprConcatStrings(CUR_POS, false, $2));  data->locations[$$] = CUR_POS; }
  }
  | SPATH {
      std::string path($1.p + 1, $1.l - 2);
      $$ = M(data, new ExprCall(CUR_POS,
          M(data, new ExprVar(data->state.symbols.create("__findFile"))),
          {M(data, new ExprVar(data->state.symbols.create("__nixPath"))),
           M(data, new ExprString(std::move(path)))}));
  }
  | URI {
      static bool noURLLiterals = experimentalFeatureSettings.isEnabled(Xp::NoUrlLiterals);
      if (noURLLiterals)
          data->error.emplace_back(nix::ErrorInfo{
              .msg = hintfmt("URL literals are disabled"),
              .errPos = data->state.positions[CUR_POS]
          });
      { $$ = M(data, new ExprString(std::string($1)));  data->locations[$$] = CUR_POS; }
  }
  | '(' expr ')' { { $$ = $2;  data->locations[$$] = CUR_POS; } }
  /* Let expressions `let {..., body = ...}' are just desugared
     into `(rec {..., body = ...}).body'. */
  | LET '{' binds '}'
    { $3->recursive = true; { $$ = M(data, new ExprSelect(noPos, $3, data->state.symbols.create("body")));  data->locations[$$] = CUR_POS; } }
  | REC '{' binds '}'
    { $3->recursive = true; { $$ = $3;  data->locations[$$] = CUR_POS; } }
  | '{' binds '}'
    { { $$ = $2;  data->locations[$$] = CUR_POS; } }
  | '[' expr_list ']' { { $$ = $2;  data->locations[$$] = CUR_POS; } }
  ;

string_parts
  : STR { { $$ = M(data, new ExprString(std::string($1)));  data->locations[$$] = CUR_POS; } }
  | string_parts_interpolated { { $$ = M(data, new ExprConcatStrings(CUR_POS, true, $1));  data->locations[$$] = CUR_POS; } }
  | { { $$ = M(data, new ExprString(""));  data->locations[$$] = CUR_POS; } }
  ;

string_parts_interpolated
  : string_parts_interpolated STR
  { { $$ = $1; $1->emplace_back(makeCurPos(@2, data), M(data, new ExprString(std::string($2))));  data->locations[$$] = CUR_POS; } }
  | string_parts_interpolated DOLLAR_CURLY expr '}' { { $$ = $1; $1->emplace_back(makeCurPos(@2, data), $3);  data->locations[$$] = CUR_POS; } }
  | DOLLAR_CURLY expr '}' { { $$ = new std::vector<std::pair<PosIdx, Expr *>>; data->SPCtx.record($$); $$->emplace_back(makeCurPos(@1, data), $2);  data->locations[$$] = CUR_POS; } }
  | STR DOLLAR_CURLY expr '}' {
      { $$ = new std::vector<std::pair<PosIdx, Expr *>>; data->SPCtx.record($$); data->locations[$$] = CUR_POS; }
      $$->emplace_back(makeCurPos(@1, data), M(data, new ExprString(std::string($1))));
      $$->emplace_back(makeCurPos(@2, data), $3);
    }
  ;

path_start
  : PATH {
    Path path(absPath({$1.p, $1.l}, data->basePath.path.abs()));
    /* add back in the trailing '/' to the first segment */
    if ($1.p[$1.l-1] == '/' && $1.l > 1)
      path += "/";
    { $$ = M(data, new ExprPath(path));  data->locations[$$] = CUR_POS; }
  }
  | HPATH {
    if (evalSettings.pureEval) {
        throw Error(
            "the path '%s' can not be resolved in pure mode",
            std::string_view($1.p, $1.l)
        );
    }
    Path path(getHome() + std::string($1.p + 1, $1.l - 1));
    { $$ = M(data, new ExprPath(path));  data->locations[$$] = CUR_POS; }
  }
  ;

ind_string_parts
  : ind_string_parts IND_STR { { $$ = $1; $1->emplace_back(makeCurPos(@2, data), $2);  data->locations[$$] = CUR_POS; } }
  | ind_string_parts DOLLAR_CURLY expr '}' { { $$ = $1; $1->emplace_back(makeCurPos(@2, data), $3);  data->locations[$$] = CUR_POS; } }
  | { { $$ = new std::vector<std::pair<PosIdx, std::variant<Expr *, StringToken>>>; data->ISPCtx.record($$);  data->locations[$$] = CUR_POS; } }
  ;

binds
  : binds attrpath '=' expr ';' { { $$ = $1; addAttr($$, std::move(*$2), $4, makeCurPos(@2, data), *data);   data->locations[$$] = CUR_POS; } }
  | binds INHERIT attrs ';'
    { { $$ = $1;  data->locations[$$] = CUR_POS; }
      for (auto & i : *$3) {
          if ($$->attrs.find(i.symbol) != $$->attrs.end())
              dupAttr(*data, i.symbol, makeCurPos(@3, data), $$->attrs[i.symbol].pos);
          auto pos = makeCurPos(@3, data);
          $$->attrs.emplace(i.symbol, ExprAttrs::AttrDef(M(data, new ExprVar(CUR_POS, i.symbol)), pos, true));
      }

    }
  | binds INHERIT '(' expr ')' attrs ';'
    { { $$ = $1;  data->locations[$$] = CUR_POS; }
      /* !!! Should ensure sharing of the expression in $4. */
      for (auto & i : *$6) {
          if ($$->attrs.find(i.symbol) != $$->attrs.end())
              dupAttr(*data, i.symbol, makeCurPos(@6, data), $$->attrs[i.symbol].pos);
          $$->attrs.emplace(i.symbol, ExprAttrs::AttrDef(M(data, new ExprSelect(CUR_POS, $4, i.symbol)), makeCurPos(@6, data)));
      }

    }
  | { { $$ = M(data, new ExprAttrs(makeCurPos(@0, data)));  data->locations[$$] = CUR_POS; } }
  ;

attrs
  : attrs attr { { $$ = $1; $1->push_back(AttrName(data->state.symbols.create($2)));  data->locations[$$] = CUR_POS; } }
  | attrs string_attr
    { { $$ = $1;  data->locations[$$] = CUR_POS; }
      ExprString * str = dynamic_cast<ExprString *>($2);
      if (str) {
          $$->push_back(AttrName(data->state.symbols.create(str->s)));

      } else
          data->error.emplace_back(nix::ErrorInfo{
              .msg = hintfmt("dynamic attributes not allowed in inherit"),
              .errPos = data->state.positions[makeCurPos(@2, data)]
          });
    }
  | { { $$ = data->APCtx.record(new AttrPath);  data->locations[$$] = CUR_POS; } }
  ;

attrpath
  : attrpath '.' attr { { $$ = $1; $1->push_back(AttrName(data->state.symbols.create($3)));  data->locations[$$] = CUR_POS; } }
  | attrpath '.' string_attr
    { { $$ = $1;  data->locations[$$] = CUR_POS; }
      ExprString * str = dynamic_cast<ExprString *>($3);
      if (str) {
          $$->push_back(AttrName(data->state.symbols.create(str->s)));

      } else
          $$->push_back(AttrName($3));
    }
  | attr {
    $$ = new std::vector<AttrName>;
    data->ANCtx.record($$);
    $$->push_back(AttrName(data->state.symbols.create($1)));
    data->locations[$$] = CUR_POS;
  }
  | string_attr
    { { $$ = new std::vector<AttrName>; data->ANCtx.record($$); data->locations[$$] = CUR_POS; }
      ExprString *str = dynamic_cast<ExprString *>($1);
      if (str) {
          $$->push_back(AttrName(data->state.symbols.create(str->s)));

      } else
          $$->push_back(AttrName($1));
    }
  ;

attr
  : ID
  | OR_KW { { $$ = {"or", 2}; } }
  ;

string_attr
  : '"' string_parts '"' { { $$ = $2;  data->locations[$$] = CUR_POS; } }
  | DOLLAR_CURLY expr '}' { { $$ = $2;  data->locations[$$] = CUR_POS; } }
  ;

expr_list
  : expr_list expr_select { { $$ = $1; $1->elems.push_back($2);  data->locations[$$] = CUR_POS; } /* !!! dangerous */ }
  | { { $$ = M(data, new ExprList);  data->locations[$$] = CUR_POS; } }
  ;

formals
  : formal ',' formals
    { { $$ = $3; $$->formals.emplace_back(*$1);   data->locations[$$] = CUR_POS; } }
  | formal
    { { $$ = new ParserFormals; data->PFCtx.record($$); $$->formals.emplace_back(*$1); $$->ellipsis = false;   data->locations[$$] = CUR_POS; } }
  |
    { { $$ = new ParserFormals; data->PFCtx.record($$); $$->ellipsis = false;  data->locations[$$] = CUR_POS; } }
  | ELLIPSIS
    { { $$ = new ParserFormals; data->PFCtx.record($$); $$->ellipsis = true;  data->locations[$$] = CUR_POS; } }
  ;

formal
  : ID { { $$ = new Formal{CUR_POS, data->state.symbols.create($1), 0}; data->FCtx.record($$); data->locations[$$] = CUR_POS; } }
  | ID '?' expr { { $$ = new Formal{CUR_POS, data->state.symbols.create($1), $3}; data->FCtx.record($$); data->locations[$$] = CUR_POS; } }
  ;

%%

#include "nixd/Parser/Epilogue.cpp"
