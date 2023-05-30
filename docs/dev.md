## Developer Guide


### Design

#### How does language information being collected?

Nix expressions are evaluated via their virtual method `nix::Expr::eval`.
Dynamic name bindings & lookups are performed in `Env`s, and the evaluation result will be stored in `Value`s.

Link: https://github.com/NixOS/nix/blob/61ddfa154bcfa522819781d23e40e984f38dfdeb/src/libexpr/nixexpr.hh#L161

So, the most important thing is to collect these information on ASTs, instead of thrown them away.

The code base achived this by inherit all Nix AST node classes, and override the virtual method `eval`, which will call super-class `eval` and then invokes a callback.

##### Why nix evaluator see your data structure, instead of parsing file by itself?

Nix parsing and evaluation are cached, and the caching interface is public.
When you open a file in your workspace, we first parse it to obtain normal Nix abstract syntax trees (ASTs).
We then recursively rewrite all AST nodes with our own data structure, creating a new tree.
Finally, we call `cacheFile` in Nix to inject our own data structure into the evaluator.


### Testing

This project is tested by "unit tests" and "regression tests".

Regression tests are written in **markdown**, and directly execute the compiled binary.
Unit tests are used for testing class interfaces, mostly public methods.

Nixd regression tests could be found at [here](tools/nixd/test/).


### Contributing

Our git history is semi-linear.
That is, firstly we rebase a branch on the top of the mainline, then merge it with `--no-ff`.

#### Commit message

Commit messages are formatted with:

```
<subsystem-name>: brief message that talks about what you changed
```

This is not strict for contributors, feel free to write your own stuff.
