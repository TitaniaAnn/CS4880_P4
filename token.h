#ifndef TOKEN_H
#define TOKEN_H

/* Token categories produced by the lexer. The parser switches on these. */
typedef enum {
    IDTk,       /* identifier: starts with 'x' (e.g. x, x1, xcount)        */
    IntTk,      /* integer literal: decimal digits only                    */
    KW_tk,      /* reserved keyword (go, int, scan, output, cond, ...)     */
    OpTk,       /* multi-char operator: ?le ?ge ?lt ?gt ?ne ?eq ** //      */
    OpDelTk,    /* single-char operator / delimiter: + - : ; = ( ) { } [ ] */
    EOFTk       /* end of input                                            */
} tokenID;

/* Human-readable names, indexed by tokenID; defined in lexer.c.
   Used by the -l (lex-only) dump and by tree printing. */
extern const char * const tokenNames[];

#define TOKEN_INSTANCE_MAX 9   /* 8 significant chars + NUL terminator */

/* One lexeme: its category, the actual text, and the line it came from. */
typedef struct {
    tokenID  id;                          /* which category (see above)   */
    char     instance[TOKEN_INSTANCE_MAX];/* the literal text of the token*/
    int      lineNumber;                  /* source line, for error msgs  */
} Token;

#endif /* TOKEN_H */
