#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"

/* Allocate a node for the given grammar label with no tokens or children. */
Node *newNode(const char *label)
{
    Node *n = malloc(sizeof(Node));
    if (!n) { fprintf(stderr, "out of memory\n"); exit(1); }
    strncpy(n->label, label, sizeof(n->label) - 1);   /* copy, bounded   */
    n->label[sizeof(n->label) - 1] = '\0';            /* always NUL-end  */
    n->tokenCount = 0;
    n->childCount = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) n->children[i] = NULL;
    return n;
}

/* Append `child` to `parent`. NULL children (from empty productions) are
   ignored, which lets parser rules call addChild() unconditionally. */
void addChild(Node *parent, Node *child)
{
    if (!child) return;
    if (parent->childCount >= MAX_CHILDREN) {
        fprintf(stderr, "internal error: too many children on '%s'\n", parent->label);
        exit(1);
    }
    parent->children[parent->childCount++] = child;
}

/* Attach a token to a node; silently drops extras past the 3-slot limit. */
void addToken(Node *n, Token t)
{
    if (n->tokenCount >= 3) return;
    n->tokens[n->tokenCount++] = t;
}

/* Render one token compactly for the debug dump (kind:text:line). */
static void printToken(Token t)
{
    switch (t.id) {
        case IDTk:    printf(" ID:%s:%d",  t.instance, t.lineNumber); break;
        case IntTk:   printf(" #:%s:%d",   t.instance, t.lineNumber); break;
        case OpTk:
        case OpDelTk: printf(" Op:%s:%d",  t.instance, t.lineNumber); break;
        default: break;   /* keywords/EOF carry no extra detail here */
    }
}

/* Print the tree with two-space indentation per level (debug aid). */
void printTree(const Node *n, int depth)
{
    if (!n) return;
    for (int i = 0; i < depth; i++) printf("  ");          /* indent      */
    printf("%s", n->label);                                /* node label  */
    for (int i = 0; i < n->tokenCount; i++) printToken(n->tokens[i]);
    printf("\n");
    for (int i = 0; i < n->childCount; i++)                /* recurse     */
        printTree(n->children[i], depth + 1);
}

/* Free a whole subtree: children first (postorder), then the node. */
void freeTree(Node *n)
{
    if (!n) return;
    for (int i = 0; i < n->childCount; i++) freeTree(n->children[i]);
    free(n);
}
