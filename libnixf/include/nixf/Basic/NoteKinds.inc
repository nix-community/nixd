/// DiagnosticNodes.inc, note declarations

#ifdef DIAG_NOTE
DIAG_NOTE("note-prev", PrevDeclared, "previously declared here")
DIAG_NOTE("merge-diff-rec-this-rec", ThisRecursive,
          "this attribute set is {}recursive")
DIAG_NOTE("merge-diff-rec-consider", RecConsider,
          "while this attribute set is marked as {}recursive, it "
          "will be considered as {}recursive")
DIAG_NOTE("note-bcomment-begin", BCommentBegin, "/* comment begins at here")
DIAG_NOTE("to-match-this", ToMachThis, "to match this {}")
#endif // DIAG_NOTE
