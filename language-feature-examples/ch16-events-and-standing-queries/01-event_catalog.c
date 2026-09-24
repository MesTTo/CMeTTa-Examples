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
 *   write checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    check_answers(claim, mt_match(catalog, vocabulary_row(words, count, true)), vocabulary_row(words, count, false));
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
    metta *m = open_engine();
    mt_space *catalog = mt_catalog(m);

    CHECK_VOCABULARY(catalog, "delivery is messaging's three promises", mt_delivery_names);
    CHECK_VOCABULARY(catalog, "event order is the second axis", mt_event_order_names);
    check_atom("an events row's delivery slot is one of them", kind_slot(catalog, "events", 3, 1),
               E("one-of", vocabulary_of(mt_delivery_names)));

    const char *promise = "eventually";
    enum mt_delivery delivery = MT_DELIVERY_AT_MOST_ONCE;
    check("C's own lookup refuses a promise no member makes",
          !mt_delivery_of(promise, &delivery) && delivery == MT_DELIVERY_AT_MOST_ONCE);
    mt_clear();
    check("and the catalog refuses it at the write", !mt_add(catalog, E("events", mt_spaceref("&feed"), promise)) && mt_error() == MT_ERROR);
    mt_clear();

    mt_space *native = mt_space_open(m, "&native-events");
    require("open &native-events", native != NULL);
    check("a native space takes a write", mt_add(native, E("reading", 1)));
    check_none("and needs no events row to be watched", mt_match(catalog, E("events", mt_spaceref(mt_space_name(native)), V("d"), V("o"))));
    mt_space_close(native);

    CHECK_VOCABULARY(catalog, "the agenda's five policies", mt_agenda_policy_names);
    check_answers("the reaction order's stated default", mt_match(catalog, E("policy", "reaction-order", V("knob"), V("default"))),
                  E("policy", "reaction-order", "agenda", mt_agenda_policy_names[MT_AGENDA_POLICY_DECLARATION]));
    check_atom("an agenda row's policy is one of them", kind_slot(catalog, "agenda", 3, 1),
               E("one-of", vocabulary_of(mt_agenda_policy_names)));
    check_atom("a reaction's priority is an optional integer", kind_slot(catalog, "on", 4, 3), E("optional", "integer"));
    return done(m);
}
