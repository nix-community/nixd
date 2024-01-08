//! Structured syntax nodes.
//! They should be able to represent any raw nix syntax.

/// Upstream type defined at [here](https://github.com/NixOS/nix/blob/8e865f3aba526394ca333efe7258bd8db0050fbb/src/libexpr/value.hh#L74)
pub type NixInt = i64;

/// Upstream type defined at [here](https://github.com/NixOS/nix/blob/8e865f3aba526394ca333efe7258bd8db0050fbb/src/libexpr/value.hh#L75)
pub type NixFloat = f64;

/// Upstream type defined at [here](https://github.com/NixOS/nix/blob/706b91ef62c80cf2a737685c74f59a69c9469c44/src/libexpr/nixexpr.hh#L210)
pub type Displacement = u32;

/// Upstream type defined at [here](https://github.com/NixOS/nix/blob/706b91ef62c80cf2a737685c74f59a69c9469c44/src/libexpr/nixexpr.hh#L211)
pub type Level = u32;

/// Evaluator position. Error message by evaluator will be reported at here.
/// The type should be sync with [upstream integer type](https://github.com/NixOS/nix/blob/706b91ef62c80cf2a737685c74f59a69c9469c44/src/libexpr/nixexpr.hh#L87)
pub struct Position {
    pub line: u32,
    pub column: u32,
}

pub enum Interpolable<'src> {
    /// The fragment should be **escaped**.
    /// This is a technical design decision, because "escaping" is tight to lexers.
    Fragment(&'src str),

    /// String interpolation. ${ expr }
    Interpolation(RawExpression<'src>),
}

/// Expressions, they can be evaluated after desugared / lowering.
pub enum RawExpression<'src> {
    String(Vec<Interpolable<'src>>),
}

/// Evaluable expressions.
/// If the nodes generated by our parser is already "legal" for evaluation,
/// just create these nodes directly.
pub enum EvaluableExpression<'src> {
    Int(NixInt),
    Float(NixFloat),

    /// String without any interpolation.
    String(&'src str),

    /// Like `String`, it cannot be interpolated.
    Path(&'src str),

    /// ### Name lookup & environment
    ///
    /// For evaluable expressions, nix may create *environment*s for variable name lookups.
    /// *Environment* is not necessary for all expressions.
    /// *Environment*s also form like a "subtree", but with a few nodes skipped.
    ///
    /// For example,
    ///
    /// ```nix
    /// let  // creates an *environment*   <-------+
    ///   x = 1;                                   |
    ///   ^ displ = 0;                             |
    ///   y = 2; // displ = 1                      |
    /// in                                         |
    /// {                                          | up 1 level
    ///   foo = let { bar = x; };                  |
    ///   //    ^           ^level = 1, displ = 0  |
    ///   //    | this creates an *environment*    |
    /// }
    /// ```
    ///
    /// Each variable has a property `from_with`, and the eval mechanism is:
    ///
    /// If `from_with`:
    ///  - Use the `name` attribute, lookup in the `level` levels up *environment*.
    ///
    /// Otherwise:
    ///  - Firstly go above `level` ups from current environment, then getting the `displ`th value.
    ///
    /// The first case is called "static lookup", and the latter is "dynamic lookup".
    ///
    /// ### Implementation notes
    ///
    /// Name lookup should be performed **statically** and associate `displ` and `level` for evaluator.
    /// e.g. `builtin`, `let ... in ...`, `rec { }`
    Var {
        pos: Position,
        name: &'src str,
        /// Whether the variable comes from an environment (e.g. a rec, let or function argument) or from a "with".
        from_with: bool,
        level: Level,
        displ: Displacement,
    },
}

pub enum Node<'src> {
    RawExpression(RawExpression<'src>),
    EvaluableExpression(EvaluableExpression<'src>),
}
