/* Purpose: a C struct becomes a record atom and comes back. Each field is
 *   mapped by hand to the atom kind that carries it, and the record read back
 *   from the space fills a struct again; the name is borrowed from the atom,
 *   so it is copied before the atom is dropped.
 * Guarantees: every field of every row round-trips
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

typedef struct person {
    char name[16];
    int64_t age;
    bool active;
} person;

static mt_atom *person_atom(const person *p)
{
    return E("Person", T(p->name), p->age, B(p->active));
}

static bool person_of(const mt_atom *atom, person *out)
{
    mt_clear();
    const char *name = mt_name(mt_at(atom, 1));
    if (mt_len(atom) != 4 || !name || strlen(name) >= sizeof out->name) return false;
    strcpy(out->name, name);
    out->age = mt_int(mt_at(atom, 2));
    out->active = mt_truth(mt_at(atom, 3));
    return mt_ok();
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const person staff[] = { { "Ada", 36, true }, { "Grace", 45, false } };
    for (size_t i = 0; i < 2; i++)
        require("store a person", mt_add(m, person_atom(&staff[i])));

    size_t found = 0;
    mt_each (row, mt_match(m, E("Person", V("name"), V("age"), V("active")))) {
        person read;
        require("read the record back", person_of(row, &read));
        for (size_t i = 0; i < 2; i++)
            if (strcmp(read.name, staff[i].name) == 0)
                assert(read.age == staff[i].age &&
                       read.active == staff[i].active
                       && "every field round-trips");
        found++;
    }
    assert((int64_t)found == 2 && "both records came back");
    mt_close(m);
    return 0;
}
