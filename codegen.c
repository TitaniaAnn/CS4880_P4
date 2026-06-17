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

static FILE *outFile;        /* destination for emitted assembly        */
static int   tempCounter;    /* next temporary number (t0, t1, ...)     */
static int   labelCounter;   /* next label number (L0, L1, ...)         */

/* A variable to declare in the data section: input variables and all
   generated temporaries both live here. */
typedef struct {
    char name[NAME_MAX];     /* variable / temporary name               */
    int  initVal;            /* initial value for the storage directive */
} VarEntry;

static VarEntry varTable[VAR_MAX];   /* everything that needs storage   */
static int      varCount;            /* number of entries               */

/* ── helpers ──────────────────────────────────────────────────────── */

/* printf to the target file, appending a newline (one instruction/line). */
static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(outFile, fmt, ap);
    va_end(ap);
    fputc('\n', outFile);
}

/* Create a fresh temporary (t0, t1, ...). It is also registered in the
   variable table so it gets a storage directive after STOP. */
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

/* Create a fresh label name (L0, L1, ...). Labels are not storage, so the
   result lives in a static buffer — use it before the next newLabel() call. */
static const char *newLabel(void)
{
    static char buf[NAME_MAX];
    snprintf(buf, NAME_MAX, "L%d", labelCounter++);
    return buf;
}

/* Record an input variable (with its declared initial value) for the
   data section. */
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

/* <R>: either a parenthesized expression, or a leaf (identifier/integer)
   that we LOAD straight into the ACC. */
static void genR(Node *n)
{
    if (n->childCount == 1) {
        genExp(n->children[0]);             /* ( <exp> ) */
    } else {
        emit("LOAD %s", n->tokens[0].instance);  /* variable or literal → ACC */
    }
}

/* <N>: a plain <R>, unary negation, or binary subtraction. Each form
   leaves its value in the ACC. */
static void genN(Node *n)
{
    if (n->tokenCount == 0) {               /* just <R>, no operator */
        genR(n->children[0]);
        return;
    }
    if (strcmp(n->children[0]->label, "N") == 0) {
        /* unary minus: - <N>  →  compute N, then 0 - N */
        char t[NAME_MAX];
        genN(n->children[0]);
        strcpy(t, newTemp());
        emit("STORE %s", t);                /* stash N so we can reload 0 */
        emit("LOAD 0");
        emit("SUB %s", t);                  /* ACC = 0 - N = -N */
    } else {
        /* binary minus: <R> - <N>. Subtraction isn't commutative, so we
           evaluate the right side first, save it, then do R - saved. */
        char t[NAME_MAX];
        genN(n->children[1]);       /* ACC = N value (RHS first) */
        strcpy(t, newTemp());
        emit("STORE %s", t);
        genR(n->children[0]);       /* ACC = R value */
        emit("SUB %s", t);          /* ACC = R - N */
    }
}

/* <M>: a plain <N>, or <N> + <M>. Result in the ACC. */
static void genM(Node *n)
{
    if (n->childCount == 1) {
        genN(n->children[0]);
        return;
    }
    /* <N> + <M>: addition is commutative, so order doesn't matter —
       compute N, save it, compute M, add the saved N back in. */
    char t[NAME_MAX];
    genN(n->children[0]);
    strcpy(t, newTemp());
    emit("STORE %s", t);
    genM(n->children[1]);
    emit("ADD %s", t);
}

/* <exp>: a plain <M>, or <M> ** <exp> / <M> // <exp>. Result in the ACC. */
static void genExp(Node *n)
{
    if (n->childCount == 1) {
        genM(n->children[0]);
        return;
    }
    char t[NAME_MAX];
    const char *op = n->tokens[0].instance;     /* "**" or "//" */

    if (strcmp(op, "**") == 0) {
        /* multiplication is commutative — evaluate either side first */
        genM(n->children[0]);
        strcpy(t, newTemp());
        emit("STORE %s", t);
        genExp(n->children[1]);
        emit("MULT %s", t);
    } else {
        /* division: need ACC = left // right (NOT commutative), so the
           right operand is evaluated and saved before the left. */
        genExp(n->children[1]);     /* ACC = right (RHS first) */
        strcpy(t, newTemp());
        emit("STORE %s", t);
        genM(n->children[0]);       /* ACC = left */
        emit("DIV %s", t);          /* ACC = left / right */
    }
}

/* ── variable declaration collector ──────────────────────────────── */

/* Walk a <vars>/<varList> chain and register each "id = integer" pair for
   the data section. Produces no instructions (declarations are storage only). */
static void genVars(Node *n)
{
    if (!n) return;
    if (strcmp(n->label, "vars") == 0) {
        if (n->tokenCount >= 2)        /* tokens: [id, integer] */
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

/* Generate both <cond> (else-less if) and <loop> (while), which share the
   same compare-and-branch shape. The body runs when the relation holds;
   for a loop we also jump back to re-test it. */
static void genCondOrLoop(Node *n, int isLoop)
{
    /* tokens[0] = IDTk (LHS variable)
       children[0] = relational
       children[1] = exp (RHS)
       children[2] = stat (body) */
    char loopTop[NAME_MAX], exitLabel[NAME_MAX], t[NAME_MAX];

    if (isLoop) {
        strcpy(loopTop, newLabel());        /* re-test point for the loop */
        emit("%s: NOOP", loopTop);
    }
    strcpy(exitLabel, newLabel());          /* jumped to when test fails  */

    /* Compute ACC = LHS - RHS so the sign encodes the comparison result. */
    genExp(n->children[1]);                         /* ACC = RHS */
    strcpy(t, newTemp());
    emit("STORE %s", t);                            /* save RHS */
    emit("LOAD %s", n->tokens[0].instance);         /* ACC = LHS */
    emit("SUB %s", t);                              /* ACC = LHS - RHS */

    /* Branch OUT of the body when the relation is false. Equality needs two
       branches (skip on negative and on positive). */
    int needTwo;
    const char *brInstr = relBranchInstr(n->children[0], &needTwo);
    emit("%s %s", brInstr, exitLabel);
    if (needTwo)
        emit("BRPOS %s", exitLabel);

    genNode(n->children[2]);                        /* body */

    if (isLoop)
        emit("BR %s", loopTop);                     /* back-edge to re-test */

    emit("%s: NOOP", exitLabel);                    /* fall-through target  */
}

/* ── main dispatcher ──────────────────────────────────────────────── */

/* Recursive dispatch on node label. Structural nodes just recurse; the
   code-generating ones emit instructions. childCount distinguishes the
   "with declarations" vs "without" shapes of program/block (vars() may
   return NULL, so the <vars> child is simply absent when empty). */
static void genNode(Node *n)
{
    if (!n) return;

    if (strcmp(n->label, "program") == 0) {
        /* go <vars> <block> exit */
        if (n->childCount == 2) {
            genVars(n->children[0]);   /* collect globals first */
            genNode(n->children[1]);   /* then the block        */
        } else {
            genNode(n->children[0]);   /* no top-level vars     */
        }
        emit("STOP");                  /* end of the instruction stream */
        /* Data section: every input variable and temporary, in order. */
        for (int i = 0; i < varCount; i++)
            emit("%s %d", varTable[i].name, varTable[i].initVal);
    }
    else if (strcmp(n->label, "block") == 0) {
        /* { <vars> <stats> } — local vars are still allocated globally */
        if (n->childCount == 2) {
            genVars(n->children[0]);
            genNode(n->children[1]);
        } else {
            genNode(n->children[0]);
        }
    }
    else if (strcmp(n->label, "stats") == 0) {
        genNode(n->children[0]);       /* <stat>  */
        genNode(n->children[1]);       /* <mStat> */
    }
    else if (strcmp(n->label, "mStat") == 0) {
        if (n->childCount == 0) return;/* empty production */
        genNode(n->children[0]);
        genNode(n->children[1]);
    }
    else if (strcmp(n->label, "stat") == 0) {
        genNode(n->children[0]);       /* unwrap to the concrete statement */
    }
    else if (strcmp(n->label, "read") == 0) {
        emit("READ %s", n->tokens[0].instance);  /* scan id → READ id */
    }
    else if (strcmp(n->label, "print") == 0) {
        /* output <exp>: evaluate to ACC, stash in a temp, then WRITE it
           (WRITE takes a memory operand, not the ACC directly). */
        char t[NAME_MAX];
        genExp(n->children[0]);
        strcpy(t, newTemp());
        emit("STORE %s", t);
        emit("WRITE %s", t);
    }
    else if (strcmp(n->label, "assign") == 0) {
        /* set id = <exp>: evaluate RHS into ACC, store into the variable. */
        genExp(n->children[0]);
        emit("STORE %s", n->tokens[0].instance);
    }
    else if (strcmp(n->label, "cond") == 0) {
        genCondOrLoop(n, 0);           /* if without else */
    }
    else if (strcmp(n->label, "loop") == 0) {
        genCondOrLoop(n, 1);           /* while loop */
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

/* Reset all module state and generate code for the whole program. */
void codeGen(Node *root, FILE *out)
{
    outFile      = out;
    tempCounter  = 0;
    labelCounter = 0;
    varCount     = 0;

    genNode(root);
}
