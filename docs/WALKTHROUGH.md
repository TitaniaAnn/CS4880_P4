# P4 Code Walkthrough & Review

CS 4880 Compiler Design — Code Generation phase.

This document is a guided tour of the compiler's source, a description of how
the four passes fit together, and a review of behavior worth understanding
before extending the code. It complements the `README` (usage, grammar, VM
target) rather than repeating it.

---

## 1. Pipeline overview

`P4` is a four-stage, single-file-at-a-time compiler. The driver in `P4.c`
wires the stages together:

```
source.ext ──> [lexer] ──> tokens ──> [parser] ──> parse tree
                                                       │
                                          [static semantics] (checks only)
                                                       │
                                                  [codegen] ──> source.asm
```

| Stage     | Files                    | Responsibility                                            |
|-----------|--------------------------|-----------------------------------------------------------|
| Lexer     | `lexer.c` / `lexer.h`    | Characters → tokens; reports lexical errors               |
| Parser    | `parser.c` / `parser.h`  | Recursive-descent build of the parse tree                 |
| Tree      | `tree.c` / `tree.h`      | Generic n-ary node type shared by parser/semantics/codegen|
| Semantics | `semantics.c`            | Declared-once / used-when-defined / unused-warning checks |
| Codegen   | `codegen.c`              | Tree walk → single-accumulator VM assembly                |
| Driver    | `P4.c`                   | Arg parsing, I/O setup, stage sequencing, cleanup         |

The data structure that connects every stage is the `Node` (`tree.h`): a label,
up to three `Token`s, and up to four children. The parser produces it; the
semantic and codegen passes consume it.

---

## 2. Lexer (`lexer.c`)

The lexer holds module-private state (`src`, `lineNum`) and exposes
`lexer_init` + `getNextToken`. Key design points:

- **Line tracking is centralized** in `nextChar`/`pushBack`, which increment and
  decrement `lineNum` as `\n` is read or pushed back. This keeps line numbers
  correct across a one-character lookahead.
- **Token line number is captured at the token's start** (`tokenLine`), so a
  multi-character token spanning a newline still reports its starting line.
- **Comments** `@...@` are skipped with a `goto restart` rather than a recursive
  `getNextToken()` call — an intentional choice (commented in code) so that a
  long run of comments doesn't grow the C stack.
- **Identifiers** must start with `x` (enforced after keyword check) and cap at
  8 significant characters; overflow is a hard lexical error rather than silent
  truncation.
- **Multi-char operators** (`?xx`, `**`, `//`) are recognized with explicit
  lookahead; a bare `?`, `*`, or `/` is a lexical error.

The keyword table (`keywords[]`) contains `og`, `then`, `func`, `program` in
addition to the keywords the grammar actually uses. They are reserved but never
accepted by the parser — see Review note R5.

---

## 3. Parser (`parser.c`)

Classic recursive descent: one `static` function per nonterminal, a single
one-token lookahead in the file-global `tk`, and `consume()` to advance.
Predicate helpers (`isKW`, `isOpDel`, `isOp`, `isStatFirst`) keep the decision
points readable.

**Tree shape.** Each rule allocates a node labeled with its nonterminal name and
attaches terminals as *tokens* and nonterminals as *children*. For example
`vars` stores the identifier and integer tokens on itself and attaches the
`varList` tail as a child.

**Expression grammar and the "delay technique".** The expression rules are
written with right recursion:

```
<exp> -> <M> ** <exp> | <M> // <exp> | <M>
<M>   -> <N> + <M> | <N>
<N>   -> - <N> | <R> - <N> | <R>
<R>   -> ( <exp> ) | identifier | integer
```

Because the recursion is on the right, the operators are **right-associative**.
For `+` and `**` this is harmless (commutative/associative). For `-` and `//`
it is *observable*:

- `10 - 4 - 3` is parsed as `10 - (4 - 3)` = **9**, not `(10 - 4) - 3` = 3.
- `100 // 10 // 2` is parsed as `100 // (10 // 2)` = **20**, not 5.

This is a property of the assigned grammar, not a bug in the parser — but it is
the single most surprising runtime behavior, so codegen mirrors it faithfully
(Section 5) and you should expect it when reading generated assembly.

The public `parser()` primes the first token, parses `<program>`, and requires
EOF afterward.

---

## 4. Static semantics (`semantics.c`)

A single pre-order walk over the tree with a flat symbol table (`stv`):

- Nodes labeled `vars`/`varList` are **definition** sites: each `IDTk` token is
  `insert()`ed (duplicate ⇒ `SEMANTIC ERROR: declared more than once`).
- Every other node's `IDTk` tokens are **uses**: `verify()` marks the entry used
  or errors `used but not defined`.
- After the walk, `checkVars()` emits a non-fatal `WARNING` for any variable
  defined but never used.

**Scoping model.** The table is global and flat — consistent with the VM target,
where "Storage is global" (README). There is no block scoping: a name declared
in any block collides with the same name anywhere else. This matches the
storage model but has two consequences worth knowing (Review notes R2/R3).

---

## 5. Code generation (`codegen.c`)

Codegen is a recursive tree walk emitting single-accumulator VM assembly. Module
state holds the output `FILE*`, the temp/label counters, and the variable table
that becomes the storage section.

**Two-part output.** `genNode` on the `program` node first walks the body
(emitting instructions), then emits `STOP`, then dumps every `VarEntry`
(user variables with their initial values, plus all compiler temporaries
initialized to 0) as storage directives.

**Expression evaluation strategy.** The machine has one accumulator, so binary
operations spill one operand to a fresh temp:

- *Commutative* ops (`+`, `**`) evaluate the left operand, `STORE` it to a temp,
  evaluate the right into ACC, then `ADD`/`MULT` the temp.
- *Non-commutative* ops (`-`, `//`) evaluate the **right** operand first, store
  it, then evaluate the left into ACC and `SUB`/`DIV` the temp — so ACC ends up
  as `left OP right`. This ordering is the crux of correctness for subtraction
  and division and is easy to get backwards.
- *Unary minus* computes the operand, then `LOAD 0` / `SUB temp` to negate.

Temps are allocated freshly and never reused; correctness comes from each having
a unique global name (see Review note R4 for the efficiency tradeoff).

**Conditionals and loops** (`genCondOrLoop`). The condition `id REL exp` is
lowered by computing `ACC = LHS - RHS` and branching to the exit label when the
condition is **false**. `relBranchInstr` maps each relational to the
false-branch instruction:

| Relational | True when | False-branch emitted |
|------------|-----------|----------------------|
| `?ge`      | ACC ≥ 0   | `BRNEG`              |
| `?le`      | ACC ≤ 0   | `BRPOS`              |
| `?gt`      | ACC > 0   | `BRZNEG`             |
| `?lt`      | ACC < 0   | `BRZPOS`             |
| `?ne`, `;` | ACC ≠ 0   | `BRZERO`             |
| `?eq`, `= =`| ACC = 0  | `BRNEG` **and** `BRPOS` |

Equality needs two false-branches (skip on negative *or* positive). A `loop`
wraps the same test in a top `Ln: NOOP` label and an unconditional `BR` back to
it before the exit label. Note `;` is treated as "not equal" (used by
`test_good11`).

---

## 6. Verification status

Reproduce locally:

```
make
for f in tests/test_good*.ext; do
  diff <(./P4 < "$f") "${f%.ext}.asm" && echo "OK ${f}"
done
```

At the time of writing:

- **Build:** clean under `-Wall -Wextra -pedantic -std=c11` (no warnings).
- **Codegen:** all 12 `tests/test_good*.ext` reproduce their committed
  `.asm` byte-for-byte.
- **Syntax errors:** all 12 `tests/test_bad*.ext` report the expected
  `SYNTAX ERROR` with line numbers.
- **Semantics:** `tests/p3gb*.ext` produce the expected duplicate/undefined
  errors and unused-variable warnings.

---

## 7. Review notes

Findings from reading the code. Most are by-design observations rather than
defects; the one clear defect (R1) is fixed in this branch.

- **R1 — stale usage string (fixed).** `P4.c` printed `Usage: P3 ...`. Corrected
  to `P4`.

- **R2 — no block scoping.** The symbol table is flat and global. A name reused
  in an inner block is reported as a redeclaration. This is consistent with the
  global-storage VM target but means the language has no shadowing.

- **R3 — definitions must precede uses in tree pre-order.** Because the semantic
  pass inserts and verifies in one pre-order walk, a use that appears earlier in
  the traversal than its definition (e.g. an outer-block statement referencing a
  variable first declared in a later sibling block) would be flagged
  `used but not defined`. Normal programs declare before use, so this is a
  latent edge case rather than an observed failure.

- **R4 — temporaries are never reused.** Every binary operation allocates a new
  `tN`. Output is correct but the storage section grows with expression
  complexity. A free-list keyed on expression depth would shrink output if that
  ever matters.

- **R5 — reserved-but-unused keywords.** `og`, `then`, `func`, `program` are in
  the lexer keyword table but unreachable in the grammar; they can only ever
  produce a syntax error. Harmless, but a reader may expect them to mean
  something.

- **R6 — silent token drop.** `addToken` (`tree.c`) silently ignores tokens past
  the third. No current rule attaches more than two, so this never triggers, but
  it would mask a bug if a future rule over-attached.

- **R7 — `newLabel` returns a static buffer.** `genCondOrLoop` immediately
  `strcpy`s each label into a local array, so there is no aliasing bug today.
  Callers must keep copying rather than holding two `newLabel()` pointers at
  once.

- **R8 — associativity of `-` and `//`.** As described in Section 3, both are
  right-associative by grammar. Faithfully implemented; just non-obvious to
  anyone reading the emitted assembly.
