#ifndef TREE_H
#define TREE_H

#include "token.h"

#define MAX_CHILDREN 4   /* widest grammar rule needs 4 children */

/* One AST node. `label` names the grammar nonterminal it came from
   (e.g. "exp", "cond"); `tokens` holds the relevant leaf tokens captured
   while parsing the rule (e.g. an operator or an identifier). */
typedef struct Node {
    char label[32];                       /* nonterminal name             */
    Token tokens[3];                      /* captured tokens (op, id, ...)*/
    int tokenCount;                       /* how many of `tokens` are set */
    struct Node *children[MAX_CHILDREN];  /* sub-trees, left to right     */
    int childCount;                       /* how many children are set    */
} Node;

Node *newNode(const char *label);         /* allocate an empty node        */
void  addChild(Node *parent, Node *child);/* append a child (ignores NULL) */
void  addToken(Node *n, Token t);         /* attach a token to a node      */
void  printTree(const Node *n, int depth);/* indented debug dump of a tree */
void  freeTree(Node *n);                  /* recursively free a tree       */

#endif /* TREE_H */
