[TOC]

## `libnixf`, The nix frontend

This is a nix language frontend (lexer + parser, semantic analysis) focusing on editing experience.

## Modules

Currently the package has these following components.

* Basic (Diagnostic Types, Node Types, ...)
* Parse (The parsing algorithm implementation)
* Sema (Semantic Analysis, e.g. duplicated keys)
* Bytecode (experimental)

## The `Basic` module

This module mainly exposing nixf interfaces, and it's definitions are shared among other modules.

### `Diagnostic` and Fixes

A `Diagnostic` have a set of `Note`s attached to it.
And each diagnostic has a set of fixes. Each fix has a name briefly describe the plan.
For example

* "insert ;"
* "remove ...".

And each fix has a series of `Edit`s. The removing range, and the new text to replace it.

These "fixes" could be applied automatically, and also may pop a "💡" and let you choose that fix.

### AST Nodes

Each node of AST is owned by it's parent.
The node is *immutable* right after it is constructed.

libnixf AST has some semantic information attached to it.
These semantic information, is constructed by *Semantic Actions*, in the `Sema` module.

## The `Parse` module

### The parsing algorithm

This is a recursive descent parser not completely consistent with the official one in case of error recovery.


The goal is:

* Provide consistent experience in case of the code can be correctly *evaluated*
* Provide much better experience in case of the code definitely cannot be correctly *evaluated*.

For example: the text below will be parsed differently in libnixf.

```nix
{
    a = 1  # <-- look at here
    b = 2;
}
```


In official nix parser it will be parsed as `a = (Call 1, b)`, for `a` attribute.

However in libnixf parser, since `1` will definitely NOT be evaluated to lambda.
Thus definitely you will not "Call" it.

So the result would be:


```nix
{
    a = 1  # <-- missing semicolon ";"
    b = 2; # Correctly parse this as "b = 2"
}
```
