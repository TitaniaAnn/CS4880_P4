#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantics.h"

#define STV_MAX 100

typedef struct {
    char name[9];
    int  used;
} STVEntry;

static STVEntry stv[STV_MAX];
static int      stv_size = 0;

static void insert(const char *name)
{
    for (int i = 0; i < stv_size; i++) {
        if (strcmp(stv[i].name, name) == 0) {
            fprintf(stderr, "SEMANTIC ERROR: '%s' declared more than once\n", name);
            exit(1);
        }
    }
    if (stv_size == STV_MAX) {
        fprintf(stderr, "SEMANTIC ERROR: variable table full (max %d)\n", STV_MAX);
        exit(1);
    }
    strncpy(stv[stv_size].name, name, sizeof(stv[stv_size].name) - 1);
    stv[stv_size].name[sizeof(stv[stv_size].name) - 1] = '\0';
    stv[stv_size].used = 0;
    stv_size++;
}

static void verify(const char *name)
{
    for (int i = 0; i < stv_size; i++) {
        if (strcmp(stv[i].name, name) == 0) {
            stv[i].used = 1;
            return;
        }
    }
    fprintf(stderr, "SEMANTIC ERROR: '%s' used but not defined\n", name);
    exit(1);
}

static void checkVars(void)
{
    for (int i = 0; i < stv_size; i++) {
        if (!stv[i].used)
            fprintf(stderr, "WARNING: '%s' defined but never used\n", stv[i].name);
    }
}

static int isDefNode(const char *label)
{
    return strcmp(label, "vars") == 0 || strcmp(label, "varList") == 0;
}

static void walk(Node *n)
{
    if (!n) return;

    if (isDefNode(n->label)) {
        for (int i = 0; i < n->tokenCount; i++)
            if (n->tokens[i].id == IDTk)
                insert(n->tokens[i].instance);
    } else {
        for (int i = 0; i < n->tokenCount; i++)
            if (n->tokens[i].id == IDTk)
                verify(n->tokens[i].instance);
    }

    for (int i = 0; i < n->childCount; i++)
        walk(n->children[i]);
}

void staticSemantics(Node *root)
{
    stv_size = 0;
    walk(root);
    checkVars();
}
