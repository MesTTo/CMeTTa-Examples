/* Purpose: the four forms that take a call as their argument, as C
 *   models what each answers. call, eval and reduce answer the call's value
 *   once its function exists; quote is a barrier that dissolves when its
 *   wrapper is evaluated, handing the call over as it was written; a call
 *   whose function does not exist yet is data, so every form hands it back,
 *   except call, which raises.
 * Assumes: the includer includes common.h first.
 * Guarantees: what the engine answers for each form, in every twin that
 *   includes this [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#ifndef CH20_CONTROL_FORMS_H
#define CH20_CONTROL_FORMS_H

typedef struct control_form {
    const char *name;
    bool reduces, raises_when_undefined;
} control_form;

static const control_form control_forms[] = {
    { "call", true, true },
    { "quote", false, false },
    { "eval", true, false },
    { "reduce", true, false },
};
#define CONTROL_FORMS (sizeof control_forms / sizeof *control_forms)

/* The row for a form, by its name. */
static inline const control_form *control_form_named(const char *name)
{
    for (size_t i = 0; i < CONTROL_FORMS; i++)
        if (strcmp(control_forms[i].name, name) == 0) return &control_forms[i];
    return NULL;
}

/* What FORM answers around CALL, whose value is VALUE, or NULL while the
   call's function does not exist. TAKES both. */
static inline mt_atom *answered(const control_form *form, mt_atom *call, mt_atom *value)
{
    if (form->reduces && value) {
        mt_drop(call);
        return value;
    }
    mt_drop(value);
    return call;
}
#endif
