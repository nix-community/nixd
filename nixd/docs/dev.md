## Developers' Manual


### Design

#### Memory model about nix language & nixd workers

**TLDR: Evaluation is not memory-safe and must be performed in a separeted address space.**

In the context of the Nix language, laziness refers to the evaluation strategy where expressions are not immediately evaluated when they are encountered, but rather when their values are actually needed.
This means that the evaluation of an expression is deferred until it is required to produce a result.

One consequence of lazy evaluation is the potential for circular references or circular dependencies.


Our upstream [C++ nix](https://github.com/NixOS/nix) uses a garbage collector and never actively free used memories.
Thus all evaluators should be used in "worker" processes.

<!--
digraph {
    Controller -> Worker [ label = "JSON RPC" dir = "both" ]
    Worker -> "Workspace Files"
    Controller -> "Workspace Files"
}
-->

```

┌─────────────────┐
│   Controller    │ ─┐
└─────────────────┘  │
  ▲                  │
  │                  │
  │ JSON RPC         │
  ▼                  │
┌─────────────────┐  │
│     Worker      │  │
└─────────────────┘  │
  │                  │
  │                  │
  ▼                  │
┌─────────────────┐  │
│ Workspace Files │ ◀┘
└─────────────────┘
```

#### How does language information being collected?

Nix expressions are evaluated using the `nix::Expr::eval` virtual method, with dynamic name bindings and lookups performed in `Env`s.
The resulting evaluation is stored in `Value`s.

Link: https://github.com/NixOS/nix/blob/61ddfa154bcfa522819781d23e40e984f38dfdeb/src/libexpr/nixexpr.hh#L161

It is mandatory to preserve the information for language services, rather than discarding it away (what nix itself do).
The codebase achieves this by inheriting all Nix AST node classes and overriding the virtual `eval` method to call the super-class `eval` and then invoke a custom callback.

##### Why does nix evaluator see your data structure, instead of parsing file by itself?

Nix parsing and evaluation are cached, and the caching interface is public.
When you open a file in your workspace, we first parse it to obtain normal Nix abstract syntax trees (ASTs).
We then recursively rewrite all AST nodes with our own data structure, creating a new tree.
Finally, we call `cacheFile` in Nix to inject our own data structure into the evaluator.


#### How does cross-file analysis work?

For example, you write a NixOS module:

```nix
{ config, lib, pkgs, ... }:

{
    # Some stuff
}
```

The file itself is a valid Nix expression, specifically an `ExprLambda`. However, it is important to know the arguments that are passed when invoking lambdas. This is necessary when writing NixOS configurations, as it helps to determine what can be used in `pkgs` and the library functions in `lib`.

Here's how it works:

When you open the file, `nixd` will parse it for you, and rewrite and inject it into the Nix evaluator (as mentioned earlier). Then, the top-level evaluation process begins, and the lambdas are evaluated. Our callback function is invoked, and the necessary information is collected in the callbacks.


### Testing

This project is tested by "unit tests" and "regression tests".

Regression tests are written in **markdown**, and directly execute the compiled binary.
Unit tests are used for testing class interfaces, mostly public methods.

Nixd regression tests could be found at [here](/tools/nixd/test/).


### Contributing

Our git history is semi-linear.
That is, firstly we rebase a branch on the top of the mainline, then merge it with `--no-ff`.

Our continuous integration systems will enable several **sanitizer** options to detect data race and undefined behavior in our codebase.

Please add or modify tests for your changed files (with all branches coveraged) and ensure that all tests can pass with sanitizers enabled.

#### Commit message

Commit messages are formatted with:

```
<subsystem-name>: brief message that talks about what you changed
```

This is not strict for contributors, feel free to write your own stuff.
