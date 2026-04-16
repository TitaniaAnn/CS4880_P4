#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"
#include "token.h"

const char * const tokenNames[] = {
    "IDTk", "IntTk", "KW_tk", "OpTk", "OpDelTk", "EOFTk"
};

static FILE *src     = NULL;
static int   lineNum = 1;

void lexer_init(FILE *fp)
{
    src     = fp;
    lineNum = 1;
}

static int nextChar(void)
{
    int c = fgetc(src);
    if (c == '\n')
        lineNum++;
    return c;
}

static void pushBack(int c)
{
    if (c == '\n')
        lineNum--;
    ungetc(c, src);
}

static void lexError(const char *detail)
{
    fprintf(stderr, "LEXICAL ERROR: %s line %d\n", detail, lineNum);
    exit(1);
}

static const char * const keywords[] = {
    "go", "og", "loop", "int", "exit", "scan",
    "output", "cond", "then", "set", "func", "program",
    NULL
};

static int isKeyword(const char *s)
{
    int i;
    for (i = 0; keywords[i] != NULL; i++)
        if (strcmp(s, keywords[i]) == 0)
            return 1;
    return 0;
}

static int isSingleCharOp(int c)
{
    return c == '+' || c == '-' || c == ':' || c == ';' ||
           c == '=' || c == '(' || c == ')' || c == '{' ||
           c == '}' || c == '[' || c == ']';
}

static Token makeToken(tokenID id, const char *instance, int line)
{
    Token t;
    t.id         = id;
    t.lineNumber = line;
    strncpy(t.instance, instance, TOKEN_INSTANCE_MAX - 1);
    t.instance[TOKEN_INSTANCE_MAX - 1] = '\0';
    return t;
}

Token getNextToken(void)
{
    int  c;
    int  tokenLine;
    char buf[TOKEN_INSTANCE_MAX];
    int  len;

restart:
    /* Skip whitespace, counting newlines via nextChar */
    do {
        c = nextChar();
        if (c == EOF)
            return makeToken(EOFTk, "", lineNum);
    } while (isspace(c));

    /* Capture line number at start of this token */
    tokenLine = lineNum;

    /* Comment: @...@ — no internal whitespace */
    if (c == '@') {
        int c2;
        while (1) {
            c2 = nextChar();
            if (c2 == EOF)
                lexError("unterminated comment");
            if (isspace(c2))
                lexError("whitespace inside comment");
            if (c2 == '@')
                break;
        }
        /* goto instead of recursive getNextToken() call: recursion would grow
           the stack once per consecutive comment; a label+goto is O(1) space */
        goto restart;
    }

    /* Letter: keyword or identifier (must start with 'x') */
    if (isalpha(c)) {
        len = 0;
        buf[len++] = (char)c;
        while (1) {
            int c2 = nextChar();
            if (isalpha(c2) || isdigit(c2) || c2 == '_') {
                if (len < TOKEN_INSTANCE_MAX - 1)
                    buf[len++] = (char)c2;
                else {
                    buf[len] = '\0';
                    lexError("identifier or keyword exceeds 8 characters");
                }
            } else {
                pushBack(c2);
                break;
            }
        }
        buf[len] = '\0';

        if (isKeyword(buf))
            return makeToken(KW_tk, buf, tokenLine);

        if (buf[0] != 'x') {
            char msg[64];
            snprintf(msg, sizeof(msg), "invalid identifier '%s'", buf);
            lexError(msg);
        }
        return makeToken(IDTk, buf, tokenLine);
    }

    /* Digit: integer */
    if (isdigit(c)) {
        len = 0;
        buf[len++] = (char)c;
        while (1) {
            int c2 = nextChar();
            if (isdigit(c2)) {
                if (len < TOKEN_INSTANCE_MAX - 1)
                    buf[len++] = (char)c2;
                else {
                    buf[len] = '\0';
                    lexError("integer exceeds 8 digits");
                }
            } else {
                pushBack(c2);
                break;
            }
        }
        buf[len] = '\0';
        return makeToken(IntTk, buf, tokenLine);
    }

    /* '?': multi-char operator ?le ?ge ?lt ?gt ?ne ?eq */
    if (c == '?') {
        int c2 = nextChar();
        int c3;
        if (c2 == EOF)
            lexError("bare '?' at end of input");
        c3 = nextChar();
        if (c3 == EOF)
            lexError("incomplete operator after '?'");
        buf[0] = '?';
        buf[1] = (char)c2;
        buf[2] = (char)c3;
        buf[3] = '\0';
        if (strcmp(buf, "?le") == 0 || strcmp(buf, "?ge") == 0 ||
            strcmp(buf, "?lt") == 0 || strcmp(buf, "?gt") == 0 ||
            strcmp(buf, "?ne") == 0 || strcmp(buf, "?eq") == 0)
            return makeToken(OpTk, buf, tokenLine);
        {
            char msg[32];
            snprintf(msg, sizeof(msg), "unknown operator '%s'", buf);
            lexError(msg);
        }
    }

    /* '*': must be '**' */
    if (c == '*') {
        int c2 = nextChar();
        if (c2 == '*')
            return makeToken(OpTk, "**", tokenLine);
        pushBack(c2);
        lexError("bare '*' is not a valid token");
    }

    /* '/': must be '//' */
    if (c == '/') {
        int c2 = nextChar();
        if (c2 == '/')
            return makeToken(OpTk, "//", tokenLine);
        pushBack(c2);
        lexError("bare '/' is not a valid token");
    }

    /* Single-char operators and delimiters */
    if (isSingleCharOp(c)) {
        buf[0] = (char)c;
        buf[1] = '\0';
        return makeToken(OpDelTk, buf, tokenLine);
    }

    /* Anything else is illegal */
    {
        char msg[32];
        snprintf(msg, sizeof(msg), "unexpected character '%c'", (char)c);
        lexError(msg);
    }

    /* unreachable — suppress compiler warning */
    return makeToken(EOFTk, "", lineNum);
}
