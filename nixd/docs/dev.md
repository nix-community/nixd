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
