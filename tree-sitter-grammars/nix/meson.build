libtree_sitter_nix_deps = [ tree_sitter ]

libtree_sitter_nix = library('tree_sitter_grammars'
                            , 'parser.c'
                            , 'scanner.c'
                            , dependencies: libtree_sitter_nix_deps
                            , install: true
                            )

tree_sitter_nix = declare_dependency( link_with: libtree_sitter_nix
                                    , dependencies: libtree_sitter_nix_deps)
