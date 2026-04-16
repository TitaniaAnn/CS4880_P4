#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"

Node *newNode(const char *label)
{
    Node *n = malloc(sizeof(Node));
    if (!n) { fprintf(stderr, "out of memory\n"); exit(1); }
    strncpy(n->label, label, sizeof(n->label) - 1);
    n->label[sizeof(n->label) - 1] = '\0';
    n->tokenCount = 0;
    n->childCount = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) n->children[i] = NULL;
    return n;
}

void addChild(Node *parent, Node *child)
{
    if (!child) return;
    if (parent->childCount >= MAX_CHILDREN) {
        fprintf(stderr, "internal error: too many children on '%s'\n", parent->label);
        exit(1);
    }
    parent->children[parent->childCount++] = child;
}

void addToken(Node *n, Token t)
{
    if (n->tokenCount >= 3) return;
    n->tokens[n->tokenCount++] = t;
}

static void printToken(Token t)
{
    switch (t.id) {
        case IDTk:    printf(" ID:%s:%d",  t.instance, t.lineNumber); break;
        case IntTk:   printf(" #:%s:%d",   t.instance, t.lineNumber); break;
        case OpTk:
        case OpDelTk: printf(" Op:%s:%d",  t.instance, t.lineNumber); break;
        default: break;
    }
}

void printTree(const Node *n, int depth)
{
    if (!n) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s", n->label);
    for (int i = 0; i < n->tokenCount; i++) printToken(n->tokens[i]);
    printf("\n");
    for (int i = 0; i < n->childCount; i++) printTree(n->children[i], depth + 1);
}

void freeTree(Node *n)
{
    if (!n) return;
    for (int i = 0; i < n->childCount; i++) freeTree(n->children[i]);
    free(n);
}
