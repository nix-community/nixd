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
