#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "tree.h"

/* Traverse the AST left-to-right and emit accumulator-VM assembly to `out`:
   the instruction stream, a final STOP, then the data section (input
   variables and generated temporaries). */
void codeGen(Node *root, FILE *out);

#endif /* CODEGEN_H */
