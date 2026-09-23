/* Purpose: a C struct becomes a record atom and comes back. Each field is
 *   mapped by hand to the atom kind that carries it, and the record read back
 *   from the space fills a struct again; the name is borrowed from the atom,
 *   so it is copied before the atom is dropped.
 * Guarantees: every field of every row round-trips [tested: make check;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    static const person staff[] = { { "Ada", 36, true }, { "Grace", 45, false } };
    for (size_t i = 0; i < 2; i++)
        require("store a person", mt_add(m, person_atom(&staff[i])));

    size_t found = 0;
    mt_each (row, mt_match(m, E("Person", V("name"), V("age"), V("active")))) {
        person read;
        require("read the record back", person_of(row, &read));
        for (size_t i = 0; i < 2; i++)
            if (strcmp(read.name, staff[i].name) == 0)
                check("every field round-trips", read.age == staff[i].age &&
                                                 read.active == staff[i].active);
        found++;
    }
    check_int("both records came back", (int64_t)found, 2);
    return done(m);
}
