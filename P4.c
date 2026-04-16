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

static void exitError(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

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
    int lexOnly = 0;
    int argStart = 1;

    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        lexOnly = 1;
        argStart = 2;
    }

    if (argc - argStart > 1)
        exitError("Usage: P3 [-l] [basename]");

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

    lexer_init(in);

    if (lexOnly) {
        runLexer();
    } else {
        Node *tree = parser();
        staticSemantics(tree);

        FILE *out = stdout;
        if (argc > argStart) {
            char outpath[512];
            snprintf(outpath, sizeof(outpath), "%s.asm", argv[argStart]);
            out = fopen(outpath, "w");
            if (!out) {
                char msg[560];
                snprintf(msg, sizeof(msg),
                         "Error: cannot open output file '%s'", outpath);
                freeTree(tree);
                if (in != stdin) fclose(in);
                exitError(msg);
            }
        }
        codeGen(tree, out);
        if (out != stdout) fclose(out);
        freeTree(tree);
    }

    if (in != stdin)
        fclose(in);

    return 0;
}
