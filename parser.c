#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tree.h"
#include "lexer.h"
#include "token.h"

/* ── global current token ─────────────────────────────────────────── */
static Token tk;

/* ── forward declarations for mutual recursion ────────────────────── */
static Node *program_rule(void);
static Node *vars(void);
static Node *varList(void);
static Node *block(void);
static Node *stats(void);
static Node *mStat(void);
static Node *stat(void);
static Node *read(void);
static Node *print(void);
static Node *cond(void);
static Node *loop(void);
static Node *assign(void);
static Node *relational(void);
static Node *expr(void);
static Node *M(void);
static Node *N(void);
static Node *R(void);

/* ── helpers ──────────────────────────────────────────────────────── */
static void error(const char *expected)
{
    fprintf(stderr, "SYNTAX ERROR: expected %s, got '%s' (line %d)\n",
            expected, tk.instance, tk.lineNumber);
    exit(1);
}

static void consume(void) { tk = getNextToken(); }

static int isKW(const char *kw)
{
    return tk.id == KW_tk && strcmp(tk.instance, kw) == 0;
}

static int isOpDel(const char *op)
{
    return tk.id == OpDelTk && strcmp(tk.instance, op) == 0;
}

static int isOp(const char *op)
{
    return tk.id == OpTk && strcmp(tk.instance, op) == 0;
}

static int isStatFirst(void)
{
    return isKW("scan") || isKW("output") || isKW("cond") ||
           isKW("loop") || isKW("set")    || isOpDel("{");
}

/* ── nonterminal implementations ──────────────────────────────────── */

/* <program> -> go <vars> <block> exit */
static Node *program_rule(void)
{
    Node *n = newNode("program");
    if (!isKW("go")) error("'go'");
    consume();
    addChild(n, vars());
    addChild(n, block());
    if (!isKW("exit")) error("'exit'");
    consume();
    return n;
}

/* <vars> -> empty | int identifier = integer <varList> : */
static Node *vars(void)
{
    if (!isKW("int")) return NULL;
    Node *n = newNode("vars");
    consume();                          /* int */
    if (tk.id != IDTk) error("identifier after 'int'");
    addToken(n, tk);
    consume();
    if (!isOpDel("=")) error("'='");
    consume();
    if (tk.id != IntTk) error("integer");
    addToken(n, tk);
    consume();
    addChild(n, varList());
    if (!isOpDel(":")) error("':'");
    consume();
    return n;
}

/* <varList> -> identifier = integer <varList> | empty */
static Node *varList(void)
{
    Node *n = newNode("varList");
    if (tk.id != IDTk) return n;           /* empty production */
    addToken(n, tk);
    consume();
    if (!isOpDel("=")) error("'='");
    consume();
    if (tk.id != IntTk) error("integer");
    addToken(n, tk);
    consume();
    addChild(n, varList());
    return n;
}

/* <block> -> { <vars> <stats> } */
static Node *block(void)
{
    if (!isOpDel("{")) error("'{'");
    Node *n = newNode("block");
    consume();
    addChild(n, vars());
    addChild(n, stats());
    if (!isOpDel("}")) error("'}'");
    consume();
    return n;
}

/* <stats> -> <stat> <mStat> */
static Node *stats(void)
{
    Node *n = newNode("stats");
    addChild(n, stat());
    addChild(n, mStat());
    return n;
}

/* <mStat> -> empty | <stat> <mStat> */
static Node *mStat(void)
{
    Node *n = newNode("mStat");
    if (!isStatFirst()) return n;          /* empty production */
    addChild(n, stat());
    addChild(n, mStat());
    return n;
}

/* <stat> -> <read> | <print> | <block> | <cond> | <loop> | <assign> */
static Node *stat(void)
{
    Node *n = newNode("stat");
    if      (isKW("scan"))   addChild(n, read());
    else if (isKW("output")) addChild(n, print());
    else if (isOpDel("{"))   addChild(n, block());
    else if (isKW("cond"))   addChild(n, cond());
    else if (isKW("loop"))   addChild(n, loop());
    else if (isKW("set"))    addChild(n, assign());
    else                     error("statement keyword or '{'");
    return n;
}

/* <read> -> scan identifier : */
static Node *read(void)
{
    Node *n = newNode("read");
    consume();                          /* scan */
    if (tk.id != IDTk) error("identifier after 'scan'");
    addToken(n, tk);
    consume();
    if (!isOpDel(":")) error("':'");
    consume();
    return n;
}

/* <print> -> output <exp> : */
static Node *print(void)
{
    Node *n = newNode("print");
    consume();                          /* output */
    addChild(n, expr());
    if (!isOpDel(":")) error("':'");
    consume();
    return n;
}

/* <cond> -> cond [ identifier <relational> <exp> ] <stat> */
static Node *cond(void)
{
    Node *n = newNode("cond");
    consume();                          /* cond */
    if (!isOpDel("[")) error("'['");
    consume();
    if (tk.id != IDTk) error("identifier");
    addToken(n, tk);
    consume();
    addChild(n, relational());
    addChild(n, expr());
    if (!isOpDel("]")) error("']'");
    consume();
    addChild(n, stat());
    return n;
}

/* <loop> -> loop [ identifier <relational> <exp> ] <stat> */
static Node *loop(void)
{
    Node *n = newNode("loop");
    consume();                          /* loop */
    if (!isOpDel("[")) error("'['");
    consume();
    if (tk.id != IDTk) error("identifier");
    addToken(n, tk);
    consume();
    addChild(n, relational());
    addChild(n, expr());
    if (!isOpDel("]")) error("']'");
    consume();
    addChild(n, stat());
    return n;
}

/* <assign> -> set identifier = <exp> : */
static Node *assign(void)
{
    Node *n = newNode("assign");
    consume();                          /* set */
    if (tk.id != IDTk) error("identifier after 'set'");
    addToken(n, tk);
    consume();
    if (!isOpDel("=")) error("'='");
    consume();
    addChild(n, expr());
    if (!isOpDel(":")) error("':'");
    consume();
    return n;
}

/* <relational> -> ?le | ?ge | ?lt | ?gt | ?ne | ?eq | = = | ; */
static Node *relational(void)
{
    Node *n = newNode("relational");
    if (tk.id == OpTk) {
        addToken(n, tk);
        consume();
    } else if (isOpDel("=")) {
        addToken(n, tk);
        consume();
        if (!isOpDel("=")) error("second '=' in '= ='");
        addToken(n, tk);
        consume();
    } else if (isOpDel(";")) {
        addToken(n, tk);
        consume();
    } else {
        error("relational operator");
    }
    return n;
}

/* <exp> -> <M> ** <exp> | <M> // <exp> | <M>   (delay technique) */
static Node *expr(void)
{
    Node *n = newNode("exp");
    addChild(n, M());
    if (isOp("**") || isOp("//")) {
        addToken(n, tk);
        consume();
        addChild(n, expr());
    }
    return n;
}

/* <M> -> <N> + <M> | <N>   (delay technique) */
static Node *M(void)
{
    Node *n = newNode("M");
    addChild(n, N());
    if (isOpDel("+")) {
        addToken(n, tk);
        consume();
        addChild(n, M());
    }
    return n;
}

/* <N> -> - <N> | <R> - <N> | <R>   (delay technique) */
static Node *N(void)
{
    Node *n = newNode("N");
    if (isOpDel("-")) {
        addToken(n, tk);
        consume();
        addChild(n, N());
    } else {
        addChild(n, R());
        if (isOpDel("-")) {
            addToken(n, tk);
            consume();
            addChild(n, N());
        }
    }
    return n;
}

/* <R> -> ( <exp> ) | identifier | integer */
static Node *R(void)
{
    Node *n = newNode("R");
    if (isOpDel("(")) {
        consume();
        addChild(n, expr());
        if (!isOpDel(")")) error("')'");
        consume();
    } else if (tk.id == IDTk) {
        addToken(n, tk);
        consume();
    } else if (tk.id == IntTk) {
        addToken(n, tk);
        consume();
    } else {
        error("identifier, integer, or '('");
    }
    return n;
}

/* ── public entry point ───────────────────────────────────────────── */
Node *parser(void)
{
    tk = getNextToken();
    Node *tree = program_rule();
    if (tk.id != EOFTk)
        error("end of input");
    return tree;
}
