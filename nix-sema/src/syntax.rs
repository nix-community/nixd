//! Structured syntax nodes.
//! They should be able to represent any raw nix syntax.
type NixInt = i64;
type NixFloat = f64;

/// Nix AST node. Each node represents a language construct.
/// Nodes are not necessary to be evaluated.
pub enum Node {
    Int(NixInt),
    Float(NixFloat),
}
