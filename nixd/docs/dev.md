## Developers' Manual

### First place to make diff

See our "issues" page. And some issues are marked with [good first issue](https://github.com/nix-community/nixd/issues?q=is%3Aissue+label%3A%22good+first+issue%22+is%3Aopen).
Such issues are intentionally left for new contributors.
i.e. main developers usually won't touch them, and there are some detailed instructions about how to do.

Please comment on that issue so that it can be assigned to you, to avoid any duplicated work in the community.

### Hack nixd from source

Clone this project and `cd` into the cloned repository.

Enter the development shell, using

```
nix develop
```

> Tip: use `direnv` & the vscode extension (direnv) + clangd, gives you IDE experience in vsocde
> Write `echo "use flake" >> .envrc` should be sufficient for basic setup.

Then configure the project, using meson.

- `--buildtype=debug` is suitable for most developers.
- `--prefix=` is used for non-privileged installation[^prefix-ignore].
- `--default-library=static` enable static linking for `libnixf`, `libnixt`, ... thus you can avoid RPATH issues in custom installation prefix.
- `-Dwerror=true` to make sure you can pass nixd CI (we enable Werror in testing env).

[^prefix-ignore]: after the installation, your worktree is tidy. A trick to resolve this is: `echo "local" >> .git/info/exclude`.

```
meson setup build --buildtype=debug --default-library=static --prefix=$PWD/local/install -Dwarning_level=3 -Dwerror=true
```

Finally, invoke

```
meson compile -C build
```

And

```
ninja -C build install
```

Then you can launch an editor to quick test nixd, under `local/install/bin`.

> [!WARNING]
> `nixd` cannot be properly launched from "build" directory (i.e. it requires installation for editor testing).

To run unit/regression test, invoke:

```
ninja -C build test
```

Remember to make sure unit/regression tests are passing before submiting PRs!

Happy hacking!

### Design

#### Memory model about nix language & nixd workers

**TLDR: Evaluation is not memory-safe and must be performed in a separated address space.**

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
