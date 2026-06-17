#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "tree.h"

/* Walk the AST and enforce the static rules: a variable must be declared
   before use and at most once (errors → exit), and every declared variable
   should be used (warning only). */
void staticSemantics(Node *root);

#endif /* SEMANTICS_H */
