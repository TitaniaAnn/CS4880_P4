#ifndef TREE_H
#define TREE_H

#include "token.h"

#define MAX_CHILDREN 4

typedef struct Node {
    char label[32];
    Token tokens[3];
    int tokenCount;
    struct Node *children[MAX_CHILDREN];
    int childCount;
} Node;

Node *newNode(const char *label);
void  addChild(Node *parent, Node *child);
void  addToken(Node *n, Token t);
void  printTree(const Node *n, int depth);
void  freeTree(Node *n);

#endif /* TREE_H */
