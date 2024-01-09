//! This is a library for semantic analysis for nix code.
//! The library aims to be parser-agnostic, and the design decision is trasform "raw" nix syntax to evaluable nodes.
//! Transformation could be erroneous and the library try to recover from most cases,
//! and also attach information for pretty-print or lsp purposes.

use evaluable::EvaluableExpression;
use raw::RawExpression;

pub mod evaluable;
pub mod raw;

pub enum Node<'src> {
    RawExpression(RawExpression<'src>),
    EvaluableExpression(EvaluableExpression<'src>),
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(1, 1);
    }
}
