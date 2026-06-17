#ifndef PARSER_H
#define PARSER_H

#include "tree.h"

/* Run the recursive-descent parser over the lexer's token stream and
   return the root of the AST (a "program" node). Exits on syntax error. */
Node *parser(void);

#endif /* PARSER_H */
