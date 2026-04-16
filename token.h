#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    IDTk,
    IntTk,
    KW_tk,
    OpTk,
    OpDelTk,
    EOFTk
} tokenID;

extern const char * const tokenNames[];

#define TOKEN_INSTANCE_MAX 9   /* 8 significant chars + NUL */

typedef struct {
    tokenID  id;
    char     instance[TOKEN_INSTANCE_MAX];
    int      lineNumber;
} Token;

#endif /* TOKEN_H */
