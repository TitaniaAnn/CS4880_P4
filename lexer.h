#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include "token.h"

/* Associate the lexer with an open FILE and reset the line counter.
   Must be called once before getNextToken(). */
void  lexer_init(FILE *fp);

/* Return the next token from the source.  Silently skips comments.
   On lexical error, prints to stderr and exits. */
Token getNextToken(void);

#endif /* LEXER_H */
