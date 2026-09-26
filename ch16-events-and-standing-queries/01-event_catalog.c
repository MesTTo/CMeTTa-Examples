/* Purpose: the event layer's own declarations, read as catalog rows, with
 *   every word C expects taken from vocabularies.h, the header generated from
 *   the engine's (vocabulary ...) rows: C holds the delivery, event-order
 *   and agenda-policy vocabularies as enums, so each row the catalog answers
 *   is checked against enum mt_delivery's, enum mt_event_order's and enum
 *   mt_agenda_policy's name tables, and a kind whose slot is (one-of
 *   <vocabulary>) names the vocabulary by the entry of mt_vocabularies that
 *   holds that table. A delivery promise no member makes is refused twice:
 *   by C's own lookup, mt_delivery_of, and by the catalog at the write,
 *   through mt_add. A native space is watchable with nothing declared, so the
 *   catalog holds no events row for one. The reaction order's default is the
 *   declaration member, and a reaction's priority is an optional integer.
 * Guarantees: all nine claims of the original hold, with its one unasserted
 *   write checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

/* The engine's name for the vocabulary whose name table is words. */
static const char *vocabulary_of(const char *const *words)
{
    for (size_t i = 0; i < MT_VOCABULARY_COUNT(mt_vocabularies); i++)
        if (mt_vocabularies[i].words == words) return mt_vocabularies[i].name;
    require("the table is a generated vocabulary", false);
    return NULL;
}

/* (vocabulary name w...) over a name table, as a pattern of fresh variables
   and as the row C expects the catalog to hold. */
static mt_atom *vocabulary_row(const char *const *words, size_t count, bool pattern)
{
    mt_atom **items = malloc((count + 2) * sizeof *items);
    require("room for the row", items != NULL);
    items[0] = S("vocabulary");
    items[1] = S(vocabulary_of(words));
    for (size_t i = 0; i < count; i++) {
        char name[16];
        snprintf(name, sizeof name, "w%zu", i);
        items[i + 2] = pattern ? V(name) : S(words[i]);
    }
    mt_atom *row = mt_exprv(count + 2, items);
    free(items);
    return row;
}

static void check_vocabulary(mt_space *catalog, const char *claim, const char *const *words, size_t count)
{
    assert(answers_are(mt_match(catalog, vocabulary_row(words, count, true)), E(vocabulary_row(words, count, false))) && claim);
}
#define CHECK_VOCABULARY(catalog, claim, table) check_vocabulary((catalog), (claim), (table), MT_VOCABULARY_COUNT(table))

/* The slot of a (kind <name> ...) row at position slot, of arity fields. */
static mt_atom *kind_slot(mt_space *catalog, const char *kind, size_t arity, size_t slot)
{
    mt_atom **items = malloc((arity + 2) * sizeof *items);
    require("room for the pattern", items != NULL);
    items[0] = S("kind");
    items[1] = S(kind);
    for (size_t i = 0; i < arity; i++) {
        char name[16];
        snprintf(name, sizeof name, "f%zu", i);
        items[i + 2] = V(name);
    }
    mt_atom *row = mt_first(mt_match(catalog, mt_exprv(arity + 2, items)));
    free(items);
    mt_atom *field = row ? mt_keep(mt_at(row, slot + 2)) : NULL;
    mt_drop(row);
    return field;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *catalog = mt_catalog(m);

    CHECK_VOCABULARY(catalog, "delivery is messaging's three promises", mt_delivery_names);
    CHECK_VOCABULARY(catalog, "event order is the second axis", mt_event_order_names);
    assert(atom_is(kind_slot(catalog, "events", 3, 1), E("one-of", vocabulary_of(mt_delivery_names)))
           && "an events row's delivery slot is one of them");

    const char *promise = "eventually";
    enum mt_delivery delivery = MT_DELIVERY_AT_MOST_ONCE;
    assert(!mt_delivery_of(promise, &delivery) && delivery == MT_DELIVERY_AT_MOST_ONCE
           && "C's own lookup refuses a promise no member makes");
    mt_clear();
    assert(!mt_add(catalog, E("events", mt_spaceref("&feed"), promise)) && mt_error() == MT_ERROR && "and the catalog refuses it at the write");
    mt_clear();

    mt_space *native = mt_space_open(m, "&native-events");
    require("open &native-events", native != NULL);
    assert(mt_add(native, E("reading", 1)) && "a native space takes a write");
    assert(!mt_first(mt_match(catalog, E("events", mt_spaceref(mt_space_name(native)), V("d"), V("o")))) && mt_ok() && "and needs no events row to be watched");
    mt_space_close(native);

    CHECK_VOCABULARY(catalog, "the agenda's five policies", mt_agenda_policy_names);
    assert(answers_are(mt_match(catalog, E("policy", "reaction-order", V("knob"), V("default"))), E(E("policy", "reaction-order", "agenda", mt_agenda_policy_names[MT_AGENDA_POLICY_DECLARATION])))
           && "the reaction order's stated default");
    assert(atom_is(kind_slot(catalog, "agenda", 3, 1), E("one-of", vocabulary_of(mt_agenda_policy_names)))
           && "an agenda row's policy is one of them");
    assert(atom_is(kind_slot(catalog, "on", 4, 3), E("optional", "integer")) && "a reaction's priority is an optional integer");
    mt_close(m);
    return 0;
}
