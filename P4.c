#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "token.h"
#include "tree.h"
#include "semantics.h"
#include "codegen.h"

/* Print a message to stderr and abort with a failure status. */
static void exitError(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/* -l mode: pull tokens until EOF and print one per line. Used to inspect
   the lexer in isolation, without parsing or code generation. */
static void runLexer(void)
{
    Token t;
    do {
        t = getNextToken();
        if (t.id == EOFTk) {
            printf("EOFTk\n");
        } else {
            printf("Group=%s Instance=%s Line=%d\n",
                   tokenNames[t.id], t.instance, t.lineNumber);
        }
    } while (t.id != EOFTk);
}

int main(int argc, char *argv[])
{
    FILE *in = NULL;
    int lexOnly = 0;       /* set by the -l flag */
    int argStart = 1;      /* index of the first non-flag argument */

    /* Optional leading "-l" switches on lex-only mode. */
    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        lexOnly = 1;
        argStart = 2;
    }

    /* At most one positional argument (the basename) is allowed. */
    if (argc - argStart > 1)
        exitError("Usage: P4 [-l] [basename]");

    /* No basename → read from stdin; otherwise open "<basename>.ext". */
    if (argc == argStart) {
        in = stdin;
    } else {
        char infile[512];
        snprintf(infile, sizeof(infile), "%s.ext", argv[argStart]);
        in = fopen(infile, "r");
        if (!in) {
            char msg[560];
            snprintf(msg, sizeof(msg),
                     "Error: cannot open input file '%s'", infile);
            exitError(msg);
        }
    }

    lexer_init(in);   /* bind the lexer to the chosen input */

    if (lexOnly) {
        runLexer();   /* just dump tokens and stop */
    } else {
        /* Front end: build the AST, then run the static-semantics checks
           (a fatal semantic error exits here, before any output is made). */
        Node *tree = parser();
        staticSemantics(tree);

        /* Output goes to "<basename>.asm" when a basename was given,
           otherwise to stdout (handy for piping). */
        FILE *out = stdout;
        if (argc > argStart) {
            char outpath[512];
            snprintf(outpath, sizeof(outpath), "%s.asm", argv[argStart]);
            out = fopen(outpath, "w");
            if (!out) {
                char msg[560];
                snprintf(msg, sizeof(msg),
                         "Error: cannot open output file '%s'", outpath);
                freeTree(tree);                 /* clean up before exit */
                if (in != stdin) fclose(in);
                exitError(msg);
            }
        }
        codeGen(tree, out);                     /* back end: emit assembly */
        if (out != stdout) fclose(out);
        freeTree(tree);
    }

    if (in != stdin)
        fclose(in);

    return 0;
}
