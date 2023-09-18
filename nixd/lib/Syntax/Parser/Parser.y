%glr-parser
%define api.pure
%locations
%define parse.error verbose
%defines
/* %no-lines */
%parse-param { void * Scanner }
%parse-param { nixd::syntax::ParseData * Data }
%lex-param { void * Scanner }
%lex-param { nixd::syntax::ParseData * Data }
%expect 1
%expect-rr 1

%code requires {
#include "nixd/Syntax/Parser/Require.h"
}

%{
#include "Parser/Prologue.cpp"
%}

%union {
    nixd::syntax::Node *Node;
    nixd::syntax::Identifier *Identifier;
    nixd::syntax::If *If;
    nixd::syntax::Formals *Formals;
    nixd::syntax::Formal *Formal;
    nixd::syntax::Binds *Binds;
    nixd::syntax::AttrPath *AttrPath;
    nixd::syntax::Call *Call;
    nixd::syntax::ConcatStrings *ConcatStrings;
    nixd::syntax::String *String;
    nixd::syntax::InterpExpr *InterpExpr;
    nixd::syntax::IndStringParts *IndStringParts;
    nixd::syntax::SPath *SPath;
    nixd::syntax::URI *Uri;
    nixd::syntax::Attrs *Attrs;
    nixd::syntax::Attribute *Attribute;
    nixd::syntax::InheritedAttribute *IA;
    nixd::syntax::ListBody *LB;
    nixd::syntax::IndString *IndString;
    nixd::syntax::Variable *Variable;

    // Tokens
    nixd::syntax::StringToken STR;
    nixd::syntax::StringToken ID;
    nixd::syntax::StringToken PATH;
    nixd::syntax::StringToken URI;
    nix::NixInt N;
    nix::NixFloat NF;
}


%type <Node> start expr expr_function expr_if expr_op expr_select expr_simple
%type <Node> string_parts
%type <Node> string_attr
%type <String> string;
%type <IndString> ind_string;
%type <ConcatStrings> string_parts_interpolated
%type <InterpExpr> string_parts_interp_expr
%type <Identifier> identifier attr identifier_or
%type <Variable> var_or
%type <IndStringParts> ind_string_parts
%type <Formals> formals
%type <Formal> formal
%type <Binds> binds
%type <AttrPath> attrpath
%type <Attrs> attrs
%type <Node> expr_app
%type <Node> path_start
%type <SPath> spath
%type <Uri> uri
%type <IA> inherited_attribute
%type <Attribute> attribute
%type <LB> list_body
%token <ID> ID
%token <N> INT
%token <NF> FLOAT
%token <STR> STR IND_STR
%token <PATH> PATH HPATH SPATH PATH_END
%token <URI> URI
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

start: expr { Data->Result = $1; };

expr: expr_function;

expr_function
  : identifier ':' expr_function {
    auto N = decorateNode(new Function, *yylocp, *Data);
    N->Arg = $1;
    N->FunctionFormals = nullptr;
    N->Body = $3;
    $$ = N;
  }
  | '{' formals '}' ':' expr_function {
    auto N = decorateNode(new Function, *yylocp, *Data);
    N->Arg = nullptr;
    N->FunctionFormals = $2;
    N->Body = $5;
    $$ = N;
  }
  | '{' formals '}' '@' identifier ':' expr_function {
    auto N = decorateNode(new Function, *yylocp, *Data);
    N->Arg = $5;
    N->FunctionFormals = $2;
    N->Body = $7;
    $$ = N;
  }
  | identifier '@' '{' formals '}' ':' expr_function {
    auto N = decorateNode(new Function, *yylocp, *Data);
    N->Arg = $1;
    N->FunctionFormals = $4;
    N->Body = $7;
    $$ = N;
  }
  | ASSERT expr ';' expr_function {
    auto N = decorateNode(new Assert, *yylocp, *Data);
    N->Cond = $2;
    N->Body = $4;
    $$ = N;
  }
  | WITH expr ';' expr_function {
    auto N = decorateNode(new With, *yylocp, *Data);
    N->Attrs = $2;
    N->Body = $4;
    $$ = N;
  }
  | LET binds IN expr_function {
    auto N = decorateNode(new Let, *yylocp, *Data);
    N->Binds = $2;
    N->Body = $4;
    $$ = N;
  }
  | expr_if



expr_if
  : IF expr THEN expr ELSE expr {
    auto N = decorateNode(new If, *yylocp, *Data);
    N->Cond = $2;
    N->Then = $4;
    N->Else = $6;
    $$ = N;
  }
  | expr_op


expr_op
  : '!' expr_op %prec NOT {
    auto N = decorateNode(new OpNot, *yylocp, *Data);
    N->Body = $2;
    N->OpRange = mkRange(@1, *Data);
    $$ = N;
  }
  | '-' expr_op %prec NEGATE {
    auto N = decorateNode(new OpNegate, *yylocp, *Data);
    N->Body = $2;
    N->OpRange = mkRange(@1, *Data);
    $$ = N;
  }
  | expr_op EQ expr_op { $$ = mkBinOp<OpEq>($1, $3, @2, *yylocp, *Data); }
  | expr_op NEQ expr_op { $$ = mkBinOp<OpNEq>($1, $3, @2, *yylocp, *Data); }
  | expr_op '<' expr_op { $$ = mkBinOp<OpLe>($1, $3, @2, *yylocp, *Data); }
  | expr_op LEQ expr_op { $$ = mkBinOp<OpLeq>($1, $3, @2, *yylocp, *Data); }
  | expr_op '>' expr_op { $$ = mkBinOp<OpGe>($1, $3, @2, *yylocp, *Data); }
  | expr_op GEQ expr_op { $$ = mkBinOp<OpGeq>($1, $3, @2, *yylocp, *Data); }
  | expr_op AND expr_op { $$ = mkBinOp<OpAnd>($1, $3, @2, *yylocp, *Data); }
  | expr_op OR expr_op { $$ = mkBinOp<OpOr>($1, $3, @2, *yylocp, *Data); }
  | expr_op IMPL expr_op { $$ = mkBinOp<OpImpl>($1, $3, @2, *yylocp, *Data); }
  | expr_op UPDATE expr_op { $$ = mkBinOp<OpUpdate>($1, $3, @2, *yylocp, *Data); }
  | expr_op '+' expr_op { $$ = mkBinOp<OpAdd>($1, $3, @2, *yylocp, *Data); }
  | expr_op '-' expr_op { $$ = mkBinOp<OpSub>($1, $3, @2, *yylocp, *Data); }
  | expr_op '*' expr_op { $$ = mkBinOp<OpMul>($1, $3, @2, *yylocp, *Data); }
  | expr_op '/' expr_op { $$ = mkBinOp<OpDiv>($1, $3, @2, *yylocp, *Data); }
  | expr_op CONCAT expr_op { $$ = mkBinOp<OpConcatLists>($1, $3, @2, *yylocp, *Data); }
  | expr_op '?' attrpath {
    auto N = decorateNode(new OpHasAttr, *yylocp, *Data);
    N->Operand = $1;
    N->Path = $3;
    N->OpRange = mkRange(@2, *Data);
    $$ = N;
  }
  | expr_app


expr_app
  : expr_app expr_select {
    if (auto F = dynamic_cast<Call *>($1)) {
      // If $1 is already a function call, then we insert the arg list
      F->Args.emplace_back($2);
    } else {
      // Otherwise, create a new function
      auto N = decorateNode(new Call, *yylocp, *Data);
      N->Fn = $1;
      N->Args = {$2};
    }
    $$->Range = mkRange(*yylocp, *Data);
  }
  | expr_select


expr_select
  : expr_simple '.' attrpath {
    auto N = decorateNode(new Select, *yylocp, *Data);
    N->Body = $1;
    N->Path = $3;
    N->Default = nullptr;
    $$ = N;
  }
  | expr_simple '.' attrpath OR_KW expr_select {
    auto N = decorateNode(new Select, *yylocp, *Data);
    N->Body = $1;
    N->Path = $3;
    N->Default = $5;
    $$ = N;
  }
  | expr_simple var_or {
    auto N = decorateNode(new Call, *yylocp, *Data);
    N->Fn = $1;
    N->Args = {$2};
    $$ = N;
  }
  | expr_simple



expr_simple
  : identifier {
    // Note: __curPos is actually not a "variable",
    // but for now we treat as if it is,
    // and convert it to ExprPos in the lowering process.
    auto N = decorateNode(new Variable, *yylocp, *Data);
    N->ID = $1;
    $$ = N;
  }
  | INT {
    auto N = decorateNode(new Int, *yylocp, *Data);
    N->N = $1;
    $$ = N;
  }
  | FLOAT {
    auto N = decorateNode(new Float, *yylocp, *Data);
    N->NF = $1;
    $$ = N;
  }
  | '"' string_parts '"' {
    $$ = $2;
  }
  | IND_STRING_OPEN ind_string_parts IND_STRING_CLOSE {
    $$ = $2;
  }
  | path_start PATH_END
  | path_start string_parts_interpolated PATH_END
  | spath {
    $$ = $1;
  }
  | uri { $$ = $1; }
  | '(' expr ')' {
    auto N = decorateNode(new Braced, *yylocp, *Data);
    N->Body = $2;
    $$ = N;
  }
  | LET '{' binds '}' {
    auto N = decorateNode(new LegacyLet, *yylocp, *Data);
    N->AttrBinds = $3;
    $$ = N;

    Diagnostic Diag;
    Diag.Msg = "using deprecated `let' syntactic sugar `let {..., body = ...}' -> (rec {..., body = ...}).body'";
    Diag.Kind = Diagnostic::Warning;
    Diag.Range = N->Range;
    Data->Diags.emplace_back(std::move(Diag));
  }
  | REC '{' binds '}' {
    auto N = decorateNode(new AttrSet, *yylocp, *Data);
    N->AttrBinds = $3;
    N->Recursive = true;
    $$ = N;
  }
  | '{' binds '}' {
    auto N = decorateNode(new AttrSet, *yylocp, *Data);
    N->AttrBinds = $2;
    N->Recursive = false;
    $$ = N;
  }
  | '[' list_body ']' {
    auto N = decorateNode(new List, *yylocp, *Data);
    N->Body = $2;
    $$ = N;
  }

list_body
  : list_body expr_select {
    $$ = $1;
    $$->Elems.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | { $$ = decorateNode(new ListBody, *yylocp, *Data); }

string_parts
  : string { $$ = $1; }
  | string_parts_interpolated { $$ = $1; }
  | { $$ = decorateNode(new String, *yylocp, *Data); }


string_parts_interpolated
  : string_parts_interpolated string {
    $$ = $1;
    $$->SubStrings.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | string_parts_interpolated string_parts_interp_expr {
    $$ = $1;
    $$->SubStrings.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | string_parts_interp_expr {
    $$ = decorateNode(new ConcatStrings, *yylocp, *Data);
    $$->SubStrings = {$1};
  }
  | string string_parts_interp_expr {
    $$ = decorateNode(new ConcatStrings, *yylocp, *Data);
    $$->SubStrings = {$1, $2};
  }


path_start
  : PATH {
    auto N = decorateNode(new Path, *yylocp, *Data);
    N->S = std::string($1);
    $$ = N;
  }
  | HPATH {
    auto N = decorateNode(new HPath, *yylocp, *Data);
    N->S = std::string($1);
    $$ = N;
  }

// Nixd extension, nix uses the token directly
spath
  : SPATH {
    auto N = decorateNode(new SPath, *yylocp, *Data);
    N->S = std::string($1);
    $$ = N;
  }


ind_string_parts
  : ind_string_parts ind_string {
    $$ = $1;
    $$->SubStrings.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | ind_string_parts string_parts_interp_expr {
    $$ = $1;
    $$->SubStrings.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | { $$ = decorateNode(new IndStringParts, *yylocp, *Data); }

// Nixd extension, nix uses the token directly
string
  : STR {
    $$ = decorateNode(new String, *yylocp, *Data);
    $$->S = std::string($1);
  }

// Nixd extension, nix uses the token directly
ind_string
  : IND_STR {
    $$ = decorateNode(new IndString, *yylocp, *Data);
    $$->S = std::string($1);
  }

// Nixd extension
// nix: DOLLAR_CURLY expr '}'
string_parts_interp_expr
  : DOLLAR_CURLY expr '}' {
    $$ = decorateNode(new InterpExpr, *yylocp, *Data);
    $$->Body = $2;
  }

// Nixd extension, nix uses the token directly
uri
  : URI {
    $$ = decorateNode(new nixd::syntax::URI, *yylocp, *Data);
    $$->S = std::string($1);
  }

identifier_or
  : OR_KW {
    $$ = decorateNode(new Identifier, *yylocp, *Data);
    $$->Symbol = Data->State.Symbols.create("or");

    Diagnostic Diag;
    Diag.Msg = "keyword `or` used as an identifier";
    Diag.Kind = Diagnostic::Warning;
    Diag.Range = $$->Range;
    Data->Diags.emplace_back(std::move(Diag));
  }

var_or: identifier_or {
  $$ = decorateNode(new Variable, *yylocp, *Data);
  $$->ID = $1;
}

identifier
  : ID {
    $$ = decorateNode(new Identifier, *yylocp, *Data);
    $$->Symbol = Data->State.Symbols.create($1);
  }


binds
  : binds attribute {
    $$ = $1;
    $$->Attributes.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | binds inherited_attribute {
    $$ = $1;
    $$->Attributes.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | { $$ = decorateNode(new Binds, *yylocp, *Data); }

// Nixd extension
inherited_attribute
  : INHERIT attrs ';' {
    $$ = decorateNode(new InheritedAttribute, *yylocp, *Data);
    $$->InheritedAttrs = $2;
    $$->E = nullptr;
  }
  | INHERIT '(' expr ')' attrs ';' {
    $$ = decorateNode(new InheritedAttribute, *yylocp, *Data);
    $$->InheritedAttrs = $5;
    $$->E = $3;
  }

// Nixd extension
attribute
  : attrpath '=' expr ';' {
    $$ = decorateNode(new Attribute, *yylocp, *Data);
    $$->Path = $1;
    $$->Body = $3;
  }

attrs
  : attrs attr {
    $$ = $1;
    $$->Names.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | attrs string_attr {
    $$ = $1;
    $$->Names.emplace_back($2);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | {
    $$ = decorateNode(new Attrs, *yylocp, *Data);
    $$->Names = {};
  }


attrpath
  : attrpath '.' attr {
    $$ = $1;
    $$->Names.emplace_back($3);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | attrpath '.' string_attr {
    $$ = $1;
    $$->Names.emplace_back($3);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | attr {
    $$ = decorateNode(new AttrPath, *yylocp, *Data);
    $$->Names = {$1};
  }
  | string_attr {
    $$ = decorateNode(new AttrPath, *yylocp, *Data);
    $$->Names = {$1};
  }


attr
  : identifier
  | identifier_or



string_attr
  : '"' string_parts '"' { $$ = $2; }
  | string_parts_interp_expr { $$ = $1; }

formals
  : formal ',' formals {
    $$ = $3;
    $$->Formals.emplace_back($1);
    $$->Range = mkRange(*yylocp, *Data);
  }
  | formal {
    $$ = decorateNode(new Formals, *yylocp, *Data);
    $$->Formals = {$1};
    $$->Ellipsis = false;
  }
  | {
    $$ = decorateNode(new Formals, *yylocp, *Data);
    $$->Formals = {};
    $$->Ellipsis = false;
  }
  | ELLIPSIS {
    $$ = decorateNode(new Formals, *yylocp, *Data);
    $$->Formals = {};
    $$->Ellipsis = true;
  }


formal
  : identifier {
    $$ = decorateNode(new Formal, *yylocp, *Data);
    $$->ID = $1;
    $$->Default = nullptr;
  }
  | identifier '?' expr {
    $$ = decorateNode(new Formal, *yylocp, *Data);
    $$->ID = $1;
    $$->Default = $3;
  }


%%

#include "Parser/Epilogue.cpp"
