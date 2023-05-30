## Developer Guide


### Design

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
