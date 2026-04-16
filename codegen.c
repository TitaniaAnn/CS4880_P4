#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "codegen.h"
#include "tree.h"
#include "token.h"

#define VAR_MAX  200
#define NAME_MAX 16

/* ── module state ─────────────────────────────────────────────────── */

static FILE *outFile;
static int   tempCounter;
static int   labelCounter;

typedef struct {
    char name[NAME_MAX];
    int  initVal;
} VarEntry;

static VarEntry varTable[VAR_MAX];
static int      varCount;

/* ── helpers ──────────────────────────────────────────────────────── */

static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(outFile, fmt, ap);
    va_end(ap);
    fputc('\n', outFile);
}

static const char *newTemp(void)
{
    if (varCount >= VAR_MAX) {
        fprintf(stderr, "codegen: variable table full\n");
        exit(1);
    }
    snprintf(varTable[varCount].name, NAME_MAX, "t%d", tempCounter++);
    varTable[varCount].initVal = 0;
    return varTable[varCount++].name;
}

static const char *newLabel(void)
{
    static char buf[NAME_MAX];
    snprintf(buf, NAME_MAX, "L%d", labelCounter++);
    return buf;
}

static void registerVar(const char *name, int initVal)
{
    if (varCount >= VAR_MAX) {
        fprintf(stderr, "codegen: variable table full\n");
        exit(1);
    }
    strncpy(varTable[varCount].name, name, NAME_MAX - 1);
    varTable[varCount].name[NAME_MAX - 1] = '\0';
    varTable[varCount].initVal = initVal;
    varCount++;
}

/* Returns the VM branch instruction for the FALSE case.
   ACC = LHS - RHS when this is evaluated.
   Sets *needTwo when two branch instructions are required (equality). */
static const char *relBranchInstr(Node *rel, int *needTwo)
{
    *needTwo = 0;
    const char *op = rel->tokens[0].instance;

    if (rel->tokenCount == 2)               /* "= =" — two OpDelTk '=' tokens */
        { *needTwo = 1; return "BRNEG"; }

    if (strcmp(op, "?ge") == 0) return "BRNEG";
    if (strcmp(op, "?le") == 0) return "BRPOS";
    if (strcmp(op, "?gt") == 0) return "BRZNEG";
    if (strcmp(op, "?lt") == 0) return "BRZPOS";
    if (strcmp(op, "?ne") == 0) return "BRZERO";
    if (strcmp(op, ";")   == 0) return "BRZERO";
    if (strcmp(op, "?eq") == 0) { *needTwo = 1; return "BRNEG"; }

    fprintf(stderr, "codegen: unknown relational operator '%s'\n", op);
    exit(1);
}

/* ── forward declarations ─────────────────────────────────────────── */

static void genNode(Node *n);
static void genVars(Node *n);
static void genExp(Node *n);
static void genM(Node *n);
static void genN(Node *n);
static void genR(Node *n);

/* ── expression generators (all leave result in ACC) ─────────────── */

static void genR(Node *n)
{
    if (n->childCount == 1) {
        genExp(n->children[0]);
    } else {
        emit("LOAD %s", n->tokens[0].instance);
    }
}

static void genN(Node *n)
{
    if (n->tokenCount == 0) {
        genR(n->children[0]);
        return;
    }
    if (strcmp(n->children[0]->label, "N") == 0) {
        /* unary minus: - <N> */
        char t[NAME_MAX];
        genN(n->children[0]);
        strcpy(t, newTemp());
        emit("STORE %s", t);
        emit("LOAD 0");
        emit("SUB %s", t);
    } else {
        /* binary minus: <R> - <N>  (need ACC = R - N) */
        char t[NAME_MAX];
        genN(n->children[1]);       /* ACC = N value (RHS first) */
        strcpy(t, newTemp());
        emit("STORE %s", t);
        genR(n->children[0]);       /* ACC = R value */
        emit("SUB %s", t);
    }
}

static void genM(Node *n)
{
    if (n->childCount == 1) {
        genN(n->children[0]);
        return;
    }
    /* <N> + <M>: addition is commutative */
    char t[NAME_MAX];
    genN(n->children[0]);
    strcpy(t, newTemp());
    emit("STORE %s", t);
    genM(n->children[1]);
    emit("ADD %s", t);
}

static void genExp(Node *n)
{
    if (n->childCount == 1) {
        genM(n->children[0]);
        return;
    }
    char t[NAME_MAX];
    const char *op = n->tokens[0].instance;

    if (strcmp(op, "**") == 0) {
        /* multiplication is commutative */
        genM(n->children[0]);
        strcpy(t, newTemp());
        emit("STORE %s", t);
        genExp(n->children[1]);
        emit("MULT %s", t);
    } else {
        /* division: need ACC = left // right (NOT commutative) */
        genExp(n->children[1]);     /* ACC = right (RHS first) */
        strcpy(t, newTemp());
        emit("STORE %s", t);
        genM(n->children[0]);       /* ACC = left */
        emit("DIV %s", t);
    }
}

/* ── variable declaration collector ──────────────────────────────── */

static void genVars(Node *n)
{
    if (!n) return;
    if (strcmp(n->label, "vars") == 0) {
        if (n->tokenCount >= 2)
            registerVar(n->tokens[0].instance, atoi(n->tokens[1].instance));
        if (n->childCount > 0)
            genVars(n->children[0]);   /* varList child */
    } else if (strcmp(n->label, "varList") == 0) {
        if (n->tokenCount >= 2)
            registerVar(n->tokens[0].instance, atoi(n->tokens[1].instance));
        if (n->childCount > 0)
            genVars(n->children[0]);   /* recursive varList */
    }
}

/* ── conditional / loop ───────────────────────────────────────────── */

static void genCondOrLoop(Node *n, int isLoop)
{
    /* tokens[0] = IDTk (LHS variable)
       children[0] = relational
       children[1] = exp (RHS)
       children[2] = stat (body) */
    char loopTop[NAME_MAX], exitLabel[NAME_MAX], t[NAME_MAX];

    if (isLoop) {
        strcpy(loopTop, newLabel());
        emit("%s: NOOP", loopTop);
    }
    strcpy(exitLabel, newLabel());

    genExp(n->children[1]);                         /* ACC = RHS */
    strcpy(t, newTemp());
    emit("STORE %s", t);
    emit("LOAD %s", n->tokens[0].instance);         /* ACC = LHS */
    emit("SUB %s", t);                              /* ACC = LHS - RHS */

    int needTwo;
    const char *brInstr = relBranchInstr(n->children[0], &needTwo);
    emit("%s %s", brInstr, exitLabel);
    if (needTwo)
        emit("BRPOS %s", exitLabel);

    genNode(n->children[2]);                        /* body */

    if (isLoop)
        emit("BR %s", loopTop);

    emit("%s: NOOP", exitLabel);
}

/* ── main dispatcher ──────────────────────────────────────────────── */

static void genNode(Node *n)
{
    if (!n) return;

    if (strcmp(n->label, "program") == 0) {
        if (n->childCount == 2) {
            genVars(n->children[0]);
            genNode(n->children[1]);
        } else {
            genNode(n->children[0]);
        }
        emit("STOP");
        for (int i = 0; i < varCount; i++)
            emit("%s %d", varTable[i].name, varTable[i].initVal);
    }
    else if (strcmp(n->label, "block") == 0) {
        if (n->childCount == 2) {
            genVars(n->children[0]);
            genNode(n->children[1]);
        } else {
            genNode(n->children[0]);
        }
    }
    else if (strcmp(n->label, "stats") == 0) {
        genNode(n->children[0]);
        genNode(n->children[1]);
    }
    else if (strcmp(n->label, "mStat") == 0) {
        if (n->childCount == 0) return;
        genNode(n->children[0]);
        genNode(n->children[1]);
    }
    else if (strcmp(n->label, "stat") == 0) {
        genNode(n->children[0]);
    }
    else if (strcmp(n->label, "read") == 0) {
        emit("READ %s", n->tokens[0].instance);
    }
    else if (strcmp(n->label, "print") == 0) {
        char t[NAME_MAX];
        genExp(n->children[0]);
        strcpy(t, newTemp());
        emit("STORE %s", t);
        emit("WRITE %s", t);
    }
    else if (strcmp(n->label, "assign") == 0) {
        genExp(n->children[0]);
        emit("STORE %s", n->tokens[0].instance);
    }
    else if (strcmp(n->label, "cond") == 0) {
        genCondOrLoop(n, 0);
    }
    else if (strcmp(n->label, "loop") == 0) {
        genCondOrLoop(n, 1);
    }
    else if (strcmp(n->label, "vars") == 0 || strcmp(n->label, "varList") == 0) {
        /* already collected by genVars; no instructions needed */
        return;
    }
    else {
        fprintf(stderr, "codegen: unexpected node label '%s'\n", n->label);
        exit(1);
    }
}

/* ── public entry point ───────────────────────────────────────────── */

void codeGen(Node *root, FILE *out)
{
    outFile      = out;
    tempCounter  = 0;
    labelCounter = 0;
    varCount     = 0;

    genNode(root);
}
