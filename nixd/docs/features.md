## Features in nixd


### Semantic Tokens

[nixd language server](https://github.com/nix-community/nixd) tries to make some tokens looks different by sending your editor some integers.
However, types in nix language is pretty different from standard LSP types.
So, as a result, attrnames, selection, variables are colored as different integers,
but the colors may, or may not rendered properly in your editor.

#### Attribute name coloring

Colors may be different for attribute path creating nested attribute set between the path just "select"s into it.

```nix
{
    foo.bar = 1;
#   ~~~ ~~~ <----- these two tokens should be colored as same
    foo.x = 2;
#   ~~~     <----- this token, however will not be colored as same as "foo.bar"
}
```

For the attribute path `foo.bar`, it creates "nested" attribute set, under "foo".

```nix
{
    foo = {
#   ^~~  <-------- new !
        bar = 1;
        #  <------- new !
    };
}
```

And the second, `foo.x`, just make a new "entry" in "foo"

```nix
{
    foo = {
        bar = 1;
        x = 2;
        # <----  new !
    };
}
```

For example in my editor setup they looks like:

<img width="566" alt="semantic tokens, bootspec" src="https://github.com/nix-community/nixd/assets/36667224/18072790-6ded-4ab9-aaa9-022a428e5da7">


Thus you can only look at "darker" attrset, which really creates something different.

For example in this picture you can only focus `.availableKernelModules`, in `boot.initrd.availableKernelModules`.


