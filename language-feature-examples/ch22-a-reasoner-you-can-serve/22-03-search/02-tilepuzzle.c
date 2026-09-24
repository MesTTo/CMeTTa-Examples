/* Purpose: the eight-tile puzzle searched breadth-first over a queue. Its
 *   twenty-four move equations are one geometry: the blank at each of the
 *   nine cells moves up, left, right or down wherever the board has a cell
 *   there, and C writes each equation from that table, the tile it meets
 *   sliding into the blank's cell. The search is the terms it is, over
 *   lib_datastructures' queue and its add-unique-or-fail dedup. C's model is
 *   the same search over the same geometry: boards as nine bytes, a visited
 *   set indexed by each board's rank among the 9! permutations, and a count
 *   of the boards dequeued. The start board is not in the visited set at
 *   first, because the original records it with add-unique-item-or-empty,
 *   which nothing defines, so its call records nothing, and the start is
 *   met again and queued a second time: every reachable board once, 9!/2,
 *   and the start once more.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

enum { SIDE = 3, CELLS = SIDE * SIDE, BLANK = 0 };

/* The blank's moves in the order the equations are written, each a step
   in cells; reached() says where one stays on the board. */
static const struct {
    const char *name;
    int step;
} moves[] = { { "U", -SIDE }, { "L", -1 }, { "R", 1 }, { "D", SIDE } };
#define MOVES (sizeof moves / sizeof *moves)

/* The cell the blank at CELL reaches by MOVE, or -1 off the board. */
static int reached(int cell, size_t move)
{
    int to = cell + moves[move].step;
    bool sideways = moves[move].step == 1 || moves[move].step == -1;
    if (to < 0 || to >= CELLS || (sideways && to / SIDE != cell / SIDE)) return -1;
    return to;
}

/* ---- the equations ---- */

/* (= (move <before> DIR) <after>): before, the blank at CELL; after, the
   tile from TO in CELL and the blank at TO. */
static mt_atom *move_equation(int cell, int to, const char *dir)
{
    mt_atom *before[CELLS], *after[CELLS];
    for (int i = 0; i < CELLS; i++) {
        char name[8];
        snprintf(name, sizeof name, "_%d", i + 1);
        before[i] = i == cell ? S("___") : V(name);
    }
    for (int i = 0; i < CELLS; i++) {
        char name[8];
        snprintf(name, sizeof name, "_%d", (i == cell ? to : i) + 1);
        after[i] = i == to ? S("___") : V(name);
    }
    return E("=", E("move", mt_exprv(CELLS, before), dir), mt_exprv(CELLS, after));
}

/* ---- C's model ---- */

typedef struct { uint8_t cell[CELLS]; } tiles;

/* The board's rank among the 9! orderings, its Lehmer code. */
static uint32_t rank(const tiles *b)
{
    uint32_t r = 0;
    for (int i = 0; i < CELLS; i++) {
        uint32_t smaller = 0;
        for (int j = i + 1; j < CELLS; j++) smaller += b->cell[j] < b->cell[i];
        r = r * (uint32_t)(CELLS - i) + smaller;
    }
    return r;
}

/* Boards dequeued by the breadth-first search from START, the start not
   recorded as seen until a move meets it. Time: every reachable board once,
   each with at most four moves. */
static int64_t dequeued(tiles start)
{
    uint32_t permutations = 1;
    for (int i = 2; i <= CELLS; i++) permutations *= (uint32_t)i;
    uint8_t *seen = calloc(permutations / 8 + 1, 1);
    tiles *queue = malloc((permutations + 1) * sizeof *queue);
    require("room", seen && queue);
    size_t head = 0, tail = 0;
    queue[tail++] = start;
    while (head < tail) {
        tiles b = queue[head++];
        int blank = 0;
        while (b.cell[blank] != BLANK) blank++;
        for (size_t mv = 0; mv < MOVES; mv++) {
            int to = reached(blank, mv);
            if (to < 0) continue;
            tiles next = b;
            next.cell[blank] = b.cell[to], next.cell[to] = BLANK;
            uint32_t r = rank(&next);
            if (seen[r / 8] >> (r % 8) & 1) continue;
            seen[r / 8] |= (uint8_t)(1u << (r % 8));
            require("room in the queue", tail <= permutations);
            queue[tail++] = next;
        }
    }
    free(seen), free(queue);
    return (int64_t)head;
}

int main(void)
{
    metta *m = open_engine();
    for (int cell = 0; cell < CELLS; cell++)
        for (size_t mv = 0; mv < MOVES; mv++) {
            int to = reached(cell, mv);
            if (to >= 0) require("a move", mt_add(m, move_equation(cell, to, moves[mv].name)));
        }
    require("lib_datastructures", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datastructures")))));

    /* The loop: an empty queue answers the count, otherwise dequeue one
       board, queue every neighbour not yet recorded, and go on. */
    require("the base case", mt_add(m, E("=", E("bfs_loop", V("Q"), V("N0")), E("if", E("==", V("Q"), E("empty-queue")), V("N0"), E("empty")))));
    require("the step",
            mt_add(m, E("=", E("bfs_loop", V("Q"), V("N0")),
                        E("let*",
                          E(E(V("Q1"), E("once", E("dequeue", V("S"), V("Q")))),
                            E(V("Ln"), E("collapse", E("let*", E(E(V("Snew"), E("move", V("S"), V("_"))), E(V("2"), E("add-unique-or-fail", "&dup", V("Snew")))),
                                                       V("Snew")))),
                            E(V("Q2"), E("foldl", "enqueue", V("Ln"), V("Q1"))), E(V("N1"), E("+", V("N0"), 1))),
                          E("bfs_loop", V("Q2"), V("N1"))))));
    require("the start",
            mt_add(m, E("=", E("bfs_all", V("Start")),
                        E("let*", E(E(V("Pt"), E("add-unique-item-or-empty", V("Start"))), E(V("Q1"), E("enqueue", V("Start"), E("empty-queue")))),
                          E("bfs_loop", V("Q1"), 0)))));

    tiles start = { { BLANK, 1, 2, 3, 4, 5, 6, 7, 8 } };
    mt_atom *cells[CELLS];
    for (int i = 0; i < CELLS; i++) cells[i] = start.cell[i] == BLANK ? S("___") : N(start.cell[i]);
    check_answers("every board once, and the start again", mt_eval(m, E("let", V("x"), E("bfs_all", mt_exprv(CELLS, cells)), V("x"))),
                  dequeued(start));
    return done(m);
}
