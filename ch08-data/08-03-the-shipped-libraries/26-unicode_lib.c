/* Purpose: lib_unicode, held against utf8proc called from C, the library the
 *   engine's own binding links (swipl-devel packages/utf8proc/CMakeLists.txt,
 *   pkg_check_modules libutf8proc), so both sides read one database. The five
 *   forms are utf8proc's named functions, utf8proc_NFD through
 *   utf8proc_NFKC_Casefold, and the fold and every other mapping are
 *   utf8proc_map over the flags a C table names; utf8proc itself refuses the
 *   two combinations the library refuses, composing with decomposing and
 *   stripping marks with neither. A property is read off utf8proc_property_t
 *   as SWI's binding reads it: no answer for an unassigned code point, for a
 *   case mapping that maps a character to itself, or for an absent
 *   decomposition type. Graphemes are cut where
 *   utf8proc_grapheme_break_stateful breaks.
 * Build: cc 26-unicode_lib.c $(pkg-config --cflags --libs cmetta libutf8proc)
 * Assumes: libutf8proc, found through pkg-config.
 * Guarantees: all fifty-four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<utf8proc.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>

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

enum { MOST = 16 };

/* One C string from what utf8proc allocated, which C frees. */
static mt_atom *text_of(utf8proc_uint8_t *mapped)
{
    require("utf8proc answers", mapped != NULL);
    mt_atom *out = T((const char *)mapped);
    free(mapped);
    return out;
}

typedef utf8proc_uint8_t *form(const utf8proc_uint8_t *);

static mt_atom *normalized(form *f, const char *text) { return text_of(f((const utf8proc_uint8_t *)text)); }

/* utf8proc_map under flags; NULL when utf8proc refuses them. */
static mt_atom *mapped(const char *text, utf8proc_option_t flags)
{
    utf8proc_uint8_t *out = NULL;
    utf8proc_ssize_t n = utf8proc_map((const utf8proc_uint8_t *)text, 0, &out, flags | UTF8PROC_NULLTERM);
    if (n < 0) return NULL;
    return text_of(out);
}

/* unicode-map's flag names, utf8proc's options [source: swipl-devel
   packages/utf8proc/unicode4pl.c, the option mask it builds; commit=V10.1.14].
   Returns false for a name utf8proc has no option for. */
static bool flag_named(const char *name, utf8proc_option_t *flags)
{
    static const struct {
        const char *name;
        utf8proc_option_t option;
    } table[] = {
        { "stable", UTF8PROC_STABLE },   { "compat", UTF8PROC_COMPAT },       { "compose", UTF8PROC_COMPOSE },
        { "decompose", UTF8PROC_DECOMPOSE }, { "ignore", UTF8PROC_IGNORE },   { "rejectna", UTF8PROC_REJECTNA },
        { "nlf2ls", UTF8PROC_NLF2LS },   { "nlf2ps", UTF8PROC_NLF2PS },       { "nlf2lf", UTF8PROC_NLF2LF },
        { "stripcc", UTF8PROC_STRIPCC }, { "casefold", UTF8PROC_CASEFOLD },   { "charbound", UTF8PROC_CHARBOUND },
        { "lump", UTF8PROC_LUMP },       { "stripmark", UTF8PROC_STRIPMARK },
    };
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        if (strcmp(table[i].name, name) == 0) return *flags |= table[i].option, true;
    return false;
}

/* The text mapped under named flags; NULL when a name is unknown or utf8proc
   refuses the combination. */
static mt_atom *mapped_by(const char *text, const char *const *names, size_t n)
{
    utf8proc_option_t flags = 0;
    for (size_t i = 0; i < n; i++)
        if (!flag_named(names[i], &flags)) return NULL;
    return mapped(text, flags);
}

static mt_atom *codes_of(const char *text, size_t len)
{
    mt_atom *out[4 * MOST];
    size_t n = 0;
    utf8proc_int32_t c;
    for (utf8proc_ssize_t used; len; text += used, len -= (size_t)used) {
        used = utf8proc_iterate((const utf8proc_uint8_t *)text, (utf8proc_ssize_t)len, &c);
        require("well-formed UTF-8", used > 0);
        out[n++] = mt_num(c);
    }
    return mt_exprv(n, out);
}

/* A character is the number of a code point or a one-character text; -1
   for anything else, which the library refuses. */
static utf8proc_int32_t code_of(const mt_atom *x)
{
    if (mt_kind_of(x) == MT_INT) return (utf8proc_int32_t)mt_int(x);
    utf8proc_int32_t c = -1;
    if (mt_kind_of(x) != MT_TEXT) return -1;
    utf8proc_ssize_t used = utf8proc_iterate((const utf8proc_uint8_t *)mt_name(x), (utf8proc_ssize_t)mt_name_len(x), &c);
    return used > 0 && (size_t)used == mt_name_len(x) ? c : -1;
}

/* What the database answers for a property, as SWI's binding reads it
   (unicode4pl.c, unicode_property): NULL for no answer. The names are the
   library's; `known` says whether the name is one. */
static mt_atom *property(const mt_atom *character, const char *name, bool *known)
{
    utf8proc_int32_t c = code_of(character);
    const utf8proc_property_t *p = utf8proc_get_property(c);
    *known = true;
    if (!p->category) return NULL;
    if (strcmp(name, "category") == 0) return S(utf8proc_category_string(c));
    if (strcmp(name, "width") == 0) return mt_num(p->charwidth);
    /* utf8proc's decomposition types in their enum's order, spelled as the
       binding spells them, lowercased (unicode4pl.c, decomp_map). */
    static const char *const decomp[] = { NULL,    "font",     "nobreak", "initial", "medial", "final",
                                          "isolated", "circle", "super",   "sub",     "vertical", "wide",
                                          "narrow",  "small",    "square",  "fraction", "compat" };
    if (strcmp(name, "decomp-type") == 0) return p->decomp_type ? S(decomp[p->decomp_type]) : NULL;
    utf8proc_int32_t (*const cases[])(utf8proc_int32_t) = { utf8proc_toupper, utf8proc_tolower };
    const char *case_names[] = { "uppercase", "lowercase" };
    for (size_t i = 0; i < 2; i++)
        if (strcmp(name, case_names[i]) == 0) return cases[i](c) != c ? mt_num(cases[i](c)) : NULL;
    *known = false;
    return NULL;
}

/* The classes as rules over the general category [source:
   lib/lib_unicode/lib_unicode.pl, character_class/2 and in_class/2;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]: a prefix, one category,
   White_Space, the ASCII range, or any category at all. */
static bool in_class(utf8proc_int32_t c, const char *class, bool *known)
{
    static const struct {
        const char *name, *category;
        bool prefix;
    } table[] = {
        { "letter", "L", true },     { "upper", "Lu", false },  { "lower", "Ll", false },  { "title", "Lt", false },
        { "digit", "Nd", false },    { "number", "N", true },   { "mark", "M", true },     { "punctuation", "P", true },
        { "symbol", "S", true },     { "separator", "Z", true }, { "control", "Cc", false },
    };
    const utf8proc_property_t *p = utf8proc_get_property(c);
    const char *category = p->category ? utf8proc_category_string(c) : "";
    *known = true;
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        if (strcmp(class, table[i].name) == 0)
            return table[i].prefix ? category[0] == table[i].category[0] : strcmp(category, table[i].category) == 0;
    if (strcmp(class, "white-space") == 0) return category[0] == 'Z' || (c >= 9 && c <= 13) || c == 133;
    if (strcmp(class, "ascii") == 0) return c < 128;
    if (strcmp(class, "assigned") == 0) return p->category != 0;
    *known = false;
    return false;
}

/* The grapheme clusters of UAX#29, cut where utf8proc says a break falls. */
static mt_atom *graphemes(const char *text)
{
    mt_atom *out[MOST];
    size_t n = 0, len = strlen(text);
    utf8proc_int32_t previous = -1, c, state = 0;
    const char *start = text;
    for (utf8proc_ssize_t used; len; text += used, len -= (size_t)used) {
        used = utf8proc_iterate((const utf8proc_uint8_t *)text, (utf8proc_ssize_t)len, &c);
        require("well-formed UTF-8", used > 0);
        if (previous >= 0 && utf8proc_grapheme_break_stateful(previous, c, &state)) {
            out[n++] = mt_textn(start, (size_t)(text - start));
            start = text;
        }
        previous = c;
    }
    if (text > start) out[n++] = mt_textn(start, (size_t)(text - start));
    return mt_exprv(n, out);
}

static bool assigned(utf8proc_int32_t c) { return utf8proc_codepoint_valid(c) && utf8proc_get_property(c)->category != 0; }

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

/* A value C computed, or no answer when it is NULL. */
static void check_maybe(const char *claim, mt_answers *got, mt_atom *want)
{
    if (want) assert(answers_are(got, E(want)) && claim);
    else assert(!mt_first(got) && mt_ok() && claim);
}

static mt_atom *names(const char *const *at, size_t n)
{
    mt_atom *out[MOST];
    for (size_t i = 0; i < n; i++) out[i] = S(at[i]);
    return mt_exprv(n, out);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_unicode", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_unicode")))));
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));

    assert(answers_are(mt_eval(m, E("unicode-version")), E(T(utf8proc_unicode_version()))) && "the database's version");

    /* The five forms. */
    const char *e_acute = "é";
    mt_atom *nfd = normalized(utf8proc_NFD, e_acute), *nfc = normalized(utf8proc_NFC, e_acute);
    assert(answers_are(mt_eval(m, E("string-codes", E("unicode-normalize", "nfd", T(e_acute)))), E(codes_of(mt_name(nfd), mt_name_len(nfd)))) && "nfd decomposes");
    assert(answers_are(mt_eval(m, E("string-codes", E("unicode-normalize", "nfc", T(e_acute)))), E(codes_of(mt_name(nfc), mt_name_len(nfc)))) && "nfc composes");
    mt_atom *round = normalized(utf8proc_NFC, mt_name(nfd));
    assert(answers_are(mt_eval(m, E("==", E("unicode-normalize", "nfc", E("unicode-normalize", "nfd", T(e_acute))), T(e_acute))), E(B(strcmp(mt_name(round), e_acute) == 0)))
           && "nfc undoes nfd");
    assert(answers_are(mt_eval(m, E("unicode-normalize", "nfkc", T("ﬃ"))), E(normalized(utf8proc_NFKC, "ﬃ"))) && "nfkc replaces a ligature");
    assert(answers_are(mt_eval(m, E("unicode-normalize", "nfkd", T("2²"))), E(normalized(utf8proc_NFKD, "2²"))) && "nfkd a superscript");
    assert(answers_are(mt_eval(m, E("unicode-normalize", "nfc", T("2²"))), E(normalized(utf8proc_NFC, "2²"))) && "nfc keeps it");
    assert(answers_are(mt_eval(m, E("unicode-normalize", "nfkc-casefold", T("Straße"))), E(normalized(utf8proc_NFKC_Casefold, "Straße"))) && "nfkc-casefold");
    const char *forms[] = { "nfc", "nfd", "nfkc", "nfkd", "nfkc-casefold" }, *asked = "nfx";
    bool a_form = false;
    for (size_t i = 0; i < 5; i++) a_form |= strcmp(forms[i], asked) == 0;
    assert(answers_are(mt_eval(m, guarded(E("unicode-normalize", asked, T("a")))), E(verdict(a_form))) && "an unknown form is refused");

    /* Folding is not lowercasing. */
    const utf8proc_option_t fold = UTF8PROC_STABLE | UTF8PROC_CASEFOLD;
    assert(answers_are(mt_eval(m, E("unicode-casefold", T("Straße"))), E(mapped("Straße", fold))) && "casefold");
    utf8proc_int32_t lower[MOST];
    utf8proc_uint8_t bytes[4 * MOST];
    size_t used = 0;
    utf8proc_ssize_t n = utf8proc_decompose((const utf8proc_uint8_t *)"Straße", 0, lower, MOST, UTF8PROC_NULLTERM);
    require("decoded", n > 0);
    for (utf8proc_ssize_t i = 0; i < n; i++) used += (size_t)utf8proc_encode_char(utf8proc_tolower(lower[i]), bytes + used);
    assert(answers_are(mt_eval(m, E("string-lower", T("Straße"))), E(mt_textn((const char *)bytes, used))) && "string-lower lowercases");
    mt_atom *upper = mapped("HELLO", fold), *low = mapped("hello", fold);
    assert(answers_are(mt_eval(m, E("==", E("unicode-casefold", T("HELLO")), E("unicode-casefold", T("hello")))), E(B(strcmp(mt_name(upper), mt_name(low)) == 0)))
           && "folded texts compare equal");

    /* unicode-map: named flags, one pass. */
    const char *strip[] = { "compose", "stripmark" }, *both_ways[] = { "compose", "decompose" }, *bare[] = { "stripmark" },
               *lump[] = { "lump" }, *casefold[] = { "casefold" }, *unknown[] = { "nosuchflag" };
    assert(answers_are(mt_eval(m, E("unicode-map", T("café"), names(strip, 2))), E(mapped_by("café", strip, 2))) && "stripping accents");
    assert(answers_are(mt_eval(m, guarded(E("unicode-map", T("a"), names(both_ways, 2)))), E(verdict(computed(mapped_by("a", both_ways, 2)))))
           && "utf8proc refuses both directions");
    assert(answers_are(mt_eval(m, guarded(E("unicode-map", T("a"), names(bare, 1)))), E(verdict(computed(mapped_by("a", bare, 1)))))
           && "and marks stripped from no form");
    assert(answers_are(mt_eval(m, E("unicode-map", T("it’s a–b"), names(lump, 1))), E(mapped_by("it’s a–b", lump, 1))) && "lump");
    assert(answers_are(mt_eval(m, E("unicode-map", T("Hello"), names(casefold, 1))), E(mapped_by("Hello", casefold, 1))) && "casefold as a flag");
    assert(answers_are(mt_eval(m, guarded(E("unicode-map", T("a"), names(unknown, 1)))), E(verdict(computed(mapped_by("a", unknown, 1))))) && "an unknown flag");

    /* The database, a character at a time. */
    static const struct {
        const char *claim, *text;
        int64_t code;
        const char *property;
    } asks[] = {
        { "a category", "A", -1, "category" },       { "of a code point", NULL, 65, "category" },
        { "a digit's", "1", -1, "category" },          { "a space's", " ", -1, "category" },
        { "a lowercase mapping", "A", -1, "lowercase" }, { "an uppercase one", "a", -1, "uppercase" },
        { "a width", "A", -1, "width" },               { "a wide character", "漢", -1, "width" },
        { "no mapping, no answer", "A", -1, "uppercase" }, { "no decomposition, no answer", "a", -1, "decomp-type" },
    };
    for (size_t i = 0; i < sizeof asks / sizeof *asks; i++) {
        mt_atom *character = asks[i].text ? T(asks[i].text) : mt_num(asks[i].code);
        bool known;
        mt_atom *value = property(character, asks[i].property, &known);
        require("a property the library names", known);
        check_maybe(asks[i].claim, mt_eval(m, E("unicode-property", mt_keep(character), asks[i].property)), value);
        mt_drop(character);
    }
    mt_atom *a = T("a"), *ab = T("ab");
    bool known;
    mt_drop(property(a, "colour", &known));
    assert(answers_are(mt_eval(m, guarded(E("unicode-property", mt_keep(a), "colour"))), E(verdict(known))) && "an unknown property is refused");
    assert(answers_are(mt_eval(m, guarded(E("unicode-property", mt_keep(ab), "category"))), E(verdict(code_of(ab) >= 0))) && "so is a text of two characters");

    /* Classes, the database's, never the locale's. */
    static const struct {
        const char *text;
        int64_t code;
        const char *class;
    } classes[] = {
        { "a", -1, "letter" },     { "1", -1, "letter" },    { "1", -1, "digit" }, { "Ⅲ", -1, "number" },
        { " ", -1, "white-space" }, { "é", -1, "letter" },   { "é", -1, "ascii" }, { "漢", -1, "letter" },
        { NULL, 65, "upper" },     { ",", -1, "punctuation" }, { NULL, 1114111, "assigned" },
    };
    for (size_t i = 0; i < sizeof classes / sizeof *classes; i++) {
        mt_atom *character = classes[i].text ? T(classes[i].text) : mt_num(classes[i].code);
        bool in = in_class(code_of(character), classes[i].class, &known);
        require("a class the library names", known);
        assert(answers_are(mt_eval(m, E("unicode-is", mt_keep(character), classes[i].class)), E(B(in))));
        mt_drop(character);
    }
    in_class('a', "vowel", &known);
    assert(answers_are(mt_eval(m, guarded(E("unicode-is", mt_keep(a), "vowel"))), E(verdict(known))) && "an unknown class is refused");

    /* Graphemes are what a person counts. */
    mt_atom *split = normalized(utf8proc_NFD, "éx"), *clusters = graphemes(mt_name(split));
    mt_atom *points = codes_of(mt_name(split), mt_name_len(split));
    assert(answers_are(mt_eval(m, E("size-atom", E("string-chars", E("unicode-normalize", "nfd", T("éx"))))), E((int64_t)mt_len(points))) && "code points");
    assert(answers_are(mt_eval(m, E("size-atom", E("unicode-graphemes", E("unicode-normalize", "nfd", T("éx"))))), E((int64_t)mt_len(clusters))) && "graphemes");
    const mt_atom *first = mt_at(clusters, 0);
    assert(answers_are(mt_eval(m, E("string-codes", E("car-atom", E("unicode-graphemes", E("unicode-normalize", "nfd", T("éx")))))), E(codes_of(mt_name(first), mt_name_len(first))))
           && "the first grapheme holds both code points");
    assert(answers_are(mt_eval(m, E("unicode-graphemes", T("abc"))), E(graphemes("abc"))) && "one grapheme per letter");
    assert(answers_are(mt_eval(m, E("unicode-graphemes", T(""))), E(graphemes(""))) && "no text, no graphemes");

    /* Valid is assigned. */
    static const int64_t codes[] = { 97, 55296, 1114112, 1114111, 57344 };
    for (size_t i = 0; i < sizeof codes / sizeof *codes; i++) {
        assert(answers_are(mt_eval(m, E("unicode-codepoint-valid", codes[i])), E(B(codes[i] <= 0x10FFFF && assigned((utf8proc_int32_t)codes[i])))));
    }
    mt_atom *private_use = mt_num(57344), *noncharacter = mt_num(1114111);
    check_maybe("a private-use category", mt_eval(m, E("unicode-property", mt_keep(private_use), "category")), property(private_use, "category", &known));
    check_maybe("an unassigned code point has none", mt_eval(m, E("unicode-property", mt_keep(noncharacter), "category")),
                property(noncharacter, "category", &known));

    mt_atom *held[] = { nfd, nfc, round, upper, low, a, ab, split, clusters, points, private_use, noncharacter };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without utf8proc's headers the program only says what it needs. */
int main(void)
{
    fputs("26-unicode_lib.c needs utf8proc: install its development files, then build with\n"
          "cc 26-unicode_lib.c $(pkg-config --cflags --libs cmetta libutf8proc)\n", stderr);
    return 77;
}
#endif
