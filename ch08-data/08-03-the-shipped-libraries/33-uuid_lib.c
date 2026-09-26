/* Purpose: lib_uuid, held against libuuid, util-linux's C library for UUIDs:
 *   uuid_generate_random and uuid_generate_time for versions 4 and 1,
 *   uuid_generate_md5 and uuid_generate_sha1 over uuid_get_template's
 *   namespaces for versions 3 and 5, uuid_parse and uuid_unparse_lower for
 *   the text, and uuid_type, uuid_variant and uuid_time for the fields. A
 *   name is its bytes with their length, so a NUL inside one counts. C
 *   refuses what libuuid refuses: text uuid_parse will not read, a namespace
 *   uuid_get_template has no template for, and a version with no name-based
 *   generator; and bytes that are not sixteen numbers from 0 to 255.
 * Build: cc 33-uuid_lib.c $(pkg-config --cflags --libs cmetta uuid libcrypto)
 * Assumes: libuuid and libcrypto (for the shared hex), through pkg-config.
 * Guarantees: all forty-nine claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<uuid.h>) && __has_include(<openssl/evp.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>
#include "_fixtures/crypto_oracle.h"

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

static mt_atom *text_of(const uuid_t u)
{
    char out[37];
    uuid_unparse_lower(u, out);
    return T(out);
}

/* A UUID's text, read as libuuid reads it: exactly thirty-six characters,
   dashes in place, either case. */
static bool parsed(const mt_atom *text, uuid_t out)
{
    return mt_kind_of(text) == MT_TEXT && mt_name_len(text) == 36 && strlen(mt_name(text)) == 36 && uuid_parse(mt_name(text), out) == 0;
}

static const char *variant_name(const uuid_t u)
{
    static const char *const names[] = { [UUID_VARIANT_NCS] = "ncs", [UUID_VARIANT_DCE] = "rfc",
                                         [UUID_VARIANT_MICROSOFT] = "microsoft", [UUID_VARIANT_OTHER] = "future" };
    return names[uuid_variant(u)];
}

/* A version-1 UUID's time in seconds; false for any other version. */
static bool timestamp(const uuid_t u, double *seconds)
{
    struct timeval tv;
    if (uuid_type(u) != 1) return false;
    uuid_time(u, &tv);
    *seconds = (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
    return true;
}

/* A namespace is one uuid_get_template names, or a UUID's own text. */
static bool namespace_of(const mt_atom *ns, uuid_t out)
{
    if (mt_kind_of(ns) == MT_SYMBOL) {
        const uuid_t *t = uuid_get_template(mt_name(ns));
        if (t) uuid_copy(out, *t);
        return t != NULL;
    }
    return parsed(ns, out);
}

/* A name-based UUID; NULL for a version with no name-based generator or a
   namespace C cannot read. */
static mt_atom *named(int version, const mt_atom *ns, const mt_atom *name)
{
    uuid_t space, out;
    if ((version != 3 && version != 5) || !namespace_of(ns, space)) return NULL;
    (version == 3 ? uuid_generate_md5 : uuid_generate_sha1)(out, space, mt_name(name), mt_name_len(name));
    return text_of(out);
}

/* Sixteen numbers from 0 to 255 as a UUID; false for anything else. */
static bool from_bytes(const mt_atom *list, uuid_t out)
{
    if (mt_kind_of(list) != MT_EXPR || mt_len(list) != 16) return false;
    for (size_t i = 0; i < 16; i++) {
        const mt_atom *b = mt_at(list, i);
        if (mt_kind_of(b) != MT_INT || mt_int(b) < 0 || mt_int(b) > 255) return false;
        out[i] = (unsigned char)mt_int(b);
    }
    return true;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

/* The function an equation of `head` spells, read back from &self. */
static mt_atom *recipe(metta *m, mt_atom *head, const char *const *params, size_t count)
{
    mt_atom *lambda = NULL;
    mt_rows (row, mt_match(m, E("=", head, V("body")))) {
        mt_atom *names[3];
        for (size_t i = 0; i < count; i++) names[i] = mt_keep(mt_bound(row, params[i]));
        mt_drop(lambda);
        lambda = E("|->", mt_exprv(count, names), mt_keep(mt_bound(row, "body")));
    }
    require("the equation is in &self", lambda != NULL);
    mt_atom *fn = mt_one(mt_eval(m, lambda));
    require("which evaluates", fn != NULL);
    return fn;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const char *const libraries[] = { "lib_crypto", "lib_uuid", "lib_encoding", "lib_string" };
    for (size_t i = 0; i < 4; i++) require("import", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));

    /* Generated identifiers, beside C's own. */
    uuid_t random, again, timed, nil;
    uuid_generate_random(random);
    uuid_generate_random(again);
    uuid_generate_time(timed);
    uuid_clear(nil);
    double when;
    assert(answers_are(mt_eval(m, E("size-atom", E("crypto-random-bytes", 16))), E((int64_t)sizeof(uuid_t))) && "a UUID is sixteen bytes");
    assert(answers_are(mt_eval(m, E("uuid-version", E("uuid-random!"))), E((int64_t)uuid_type(random))) && "random is version 4");
    assert(answers_are(mt_eval(m, E("uuid-variant", E("uuid-random!"))), E(S(variant_name(random)))) && "of the RFC variant");
    mt_atom *random_text = text_of(random);
    uuid_t scratch;
    assert(answers_are(mt_eval(m, E("uuid-is", E("uuid-random!"))), E(B(parsed(random_text, scratch)))) && "a UUID");
    assert(answers_are(mt_eval(m, E("!=", E("uuid-random!"), E("uuid-random!"))), E(B(uuid_compare(random, again) != 0))) && "two draws differ");
    assert(answers_are(mt_eval(m, E("uuid-version", E("uuid-time!"))), E((int64_t)uuid_type(timed))) && "time-based is version 1");
    assert(answers_are(mt_eval(m, E("uuid-variant", E("uuid-time!"))), E(S(variant_name(timed)))) && "of the RFC variant too");
    assert(answers_are(mt_eval(m, E(">", E("uuid-timestamp", E("uuid-time!")), 0)), E(B(timestamp(timed, &when) && when > 0))) && "with a time");

    /* Names are deterministic: libuuid's generators over its templates. */
    static const char *const templates[] = { "dns", "url", "oid", "x500" };
    mt_atom *spaces[4];
    for (size_t i = 0; i < 4; i++) {
        uuid_t t;
        mt_atom *name = S(templates[i]);
        require("libuuid has the template", namespace_of(name, t));
        spaces[i] = name;
    }
    assert(answers_are(mt_eval(m, E("uuid-namespaces")), E(mt_exprv(4, spaces))) && "the namespaces");
    static const struct {
        int version;
        const char *ns, *name;
    } names[] = {
        { 3, "dns", "example.com" }, { 5, "dns", "example.com" }, { 3, "url", "http://example.com" }, { 5, "oid", "1.3.6.1.4.1" },
        { 5, "x500", "cn=Test" },    { 5, "dns", "" },             { 5, "dns", "é" },                  { 3, "dns", "漢" },
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        mt_atom *ns = S(names[i].ns), *name = T(names[i].name);
        assert(answers_are(mt_eval(m, E("uuid-name", names[i].version, mt_keep(ns), mt_keep(name))), E(named(names[i].version, ns, name))));
        mt_drop(ns);
        mt_drop(name);
    }
    mt_atom *dns_text = T("6ba7b810-9dad-11d1-80b4-00c04fd430c8"), *dns = S("dns"), *example = T("example.com");
    mt_atom *by_text = named(5, dns_text, example), *by_name = named(5, dns, example);
    assert(answers_are(mt_eval(m, E("==", E("uuid-name", 5, mt_keep(dns_text), mt_keep(example)), E("uuid-name", 5, mt_keep(dns), mt_keep(example)))), E(B(mt_eq(by_text, by_name))))
           && "a namespace is also its own text");
    static const char nul_name[] = { 'a', '\0', 'b' };
    mt_atom *with_nul = mt_textn(nul_name, 3), *nul_codes = E(97, 0, 98);
    assert(answers_are(mt_eval(m, E("uuid-name", 5, mt_keep(dns), E("string-from-codes", mt_keep(nul_codes)))), E(named(5, dns, with_nul)))
           && "a NUL is part of a name");

    /* The nil UUID and the variants. */
    assert(answers_are(mt_eval(m, E("uuid-nil")), E(text_of(nil))) && "nil");
    mt_atom *nil_text = text_of(nil);
    assert(answers_are(mt_eval(m, E("uuid-is", E("uuid-nil"))), E(B(parsed(nil_text, scratch)))) && "is a UUID");
    assert(answers_are(mt_eval(m, E("uuid-version", E("uuid-nil"))), E((int64_t)uuid_type(nil))) && "of version 0");
    assert(answers_are(mt_eval(m, E("uuid-variant", E("uuid-nil"))), E(S(variant_name(nil)))) && "and the NCS variant");
    static const char *const variants[] = { "00000000-0000-0000-c000-000000000000", "ffffffff-ffff-ffff-ffff-ffffffffffff" };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *text = T(variants[i]);
        uuid_t u;
        require("libuuid reads it", parsed(text, u));
        assert(answers_are(mt_eval(m, E("uuid-variant", mt_keep(text))), E(S(variant_name(u)))) && variants[i]);
        mt_drop(text);
    }
    mt_atom *epoch = T("13814000-1dd2-11b2-8000-000000000000");
    uuid_t epoch_u;
    require("libuuid reads the epoch", parsed(epoch, epoch_u) && timestamp(epoch_u, &when));
    assert(answers_are(mt_eval(m, E("uuid-timestamp", mt_keep(epoch))), E(when)) && "version 1's epoch");
    require("no time in other versions", !timestamp(random, &when) && !timestamp(nil, &when));
    assert(!mt_first(mt_eval(m, E("uuid-timestamp", E("uuid-random!")))) && mt_ok() && "a random UUID has no time");
    assert(!mt_first(mt_eval(m, E("uuid-timestamp", E("uuid-nil")))) && mt_ok() && "nor has nil");

    /* Text either case in, lower case out; all 128 bits as bytes. */
    mt_atom *upper = T("CFBFF0D1-9375-5685-968C-48CE8B15AE17"), *lower = T("cfbff0d1-9375-5685-968c-48ce8b15ae17");
    uuid_t u, all_ones;
    require("libuuid reads both cases", parsed(upper, u));
    assert(answers_are(mt_eval(m, E("uuid-is", mt_keep(upper))), E(B(parsed(upper, scratch)))) && "upper case reads");
    assert(answers_are(mt_eval(m, E("hex-encode", E("uuid-bytes", mt_keep(lower)))), E(hex(u, 16))) && "the bytes, in hex");
    assert(answers_are(mt_eval(m, E("uuid-of-bytes", E("uuid-bytes", mt_keep(upper)))), E(text_of(u))) && "and back to lower case");
    assert(answers_are(mt_eval(m, E("uuid-bytes", E("uuid-nil"))), E(mt_array(sizeof(uuid_t), nil))) && "nil's bytes");
    memset(all_ones, 0xff, sizeof all_ones);
    assert(answers_are(mt_eval(m, E("uuid-of-bytes", E("hex-decode", T("ffffffffffffffffffffffffffffffff")))), E(text_of(all_ones))) && "every bit set");

    /* What is not a UUID. */
    mt_atom *dashes = T("------------------------------------"), *nul_dashes = NULL, *bare = T("cfbff0d193755685968c48ce8b15ae17"),
            *z = T("zfbff0d1-9375-5685-968c-48ce8b15ae17"), *three = mt_num(3);
    char zeros[37];
    snprintf(zeros, sizeof zeros, "%s", mt_name(nil_text));
    for (char *c = zeros; *c; c++)
        if (*c == '-') *c = '\0';
    nul_dashes = mt_textn(zeros, 36);
    assert(answers_are(mt_eval(m, E("uuid-is", mt_keep(dashes))), E(B(parsed(dashes, scratch)))) && "dashes alone");
    assert(answers_are(mt_eval(m, E("uuid-is", E("string-replace", E("uuid-nil"), T("-"), E("string-from-codes", E(0))))), E(B(parsed(nul_dashes, scratch))))
           && "NULs for dashes");
    assert(answers_are(mt_eval(m, E("uuid-is", mt_keep(bare))), E(B(parsed(bare, scratch)))) && "no dashes");
    assert(answers_are(mt_eval(m, E("uuid-is", mt_keep(z))), E(B(parsed(z, scratch)))) && "a letter past f");
    assert(answers_are(mt_eval(m, E("uuid-is", mt_keep(three))), E(B(parsed(three, scratch)))) && "a number");
    mt_atom *broken = T("broken"), *short_bytes = E(1, 2), *wide = E(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 256), *missing = S("missing"),
            *name = T("name");
    assert(answers_are(mt_eval(m, guarded(E("uuid-version", mt_keep(broken)))), E(verdict(parsed(broken, scratch)))) && "no version of broken text");
    assert(answers_are(mt_eval(m, guarded(E("uuid-timestamp", mt_keep(broken)))), E(verdict(parsed(broken, scratch)))) && "no time of it");
    assert(answers_are(mt_eval(m, guarded(E("uuid-of-bytes", mt_keep(short_bytes)))), E(verdict(from_bytes(short_bytes, scratch)))) && "two bytes");
    assert(answers_are(mt_eval(m, guarded(E("uuid-of-bytes", mt_keep(wide)))), E(verdict(from_bytes(wide, scratch)))) && "a byte of 256");
    assert(answers_are(mt_eval(m, guarded(E("uuid-name", 2, mt_keep(dns), mt_keep(name)))), E(verdict(computed(named(2, dns, name))))) && "no name-based version 2");
    assert(answers_are(mt_eval(m, guarded(E("uuid-name", 5, mt_keep(missing), mt_keep(name)))), E(verdict(computed(named(5, missing, name))))) && "no such namespace");
    assert(answers_are(mt_eval(m, guarded(E("uuid-name", 5, mt_keep(broken), mt_keep(name)))), E(verdict(computed(named(5, broken, name)))))
           && "no namespace in broken text");

    /* The equations read back out of &self. */
    const char *id[] = { "id" }, *three_params[] = { "v", "ns", "name" };
    mt_atom *inspect = recipe(m, E("uuid-version", V("id")), id, 1);
    assert(answers_are(mt_eval(m, E(inspect, E("uuid-nil"))), E((int64_t)uuid_type(nil))) && "a version recipe");
    mt_atom *derive = recipe(m, E("uuid-name", V("v"), V("ns"), V("name")), three_params, 3);
    assert(answers_are(mt_eval(m, E(derive, 5, "dns", mt_keep(example))), E(named(5, dns, example))) && "a name recipe");
    assert(answers_are(mt_eval(m, E("collapse", E("uuid-version", E("superpose", E(T("00000000-0000-0000-0000-000000000000"), T("ffffffff-ffff-ffff-ffff-ffffffffffff")))))), E(E((int64_t)uuid_type(nil), (int64_t)uuid_type(all_ones))))
           && "one version per alternative");
    assert(answers_are(mt_eval(m, E("uuid-name", 3, mt_keep(dns), E("string-from-codes", mt_keep(nul_codes)))), E(named(3, dns, with_nul)))
           && "a NUL in a version 3 name");

    mt_atom *held[] = { random_text, dns_text, dns, example, by_text, by_name, with_nul, nul_codes, nil_text, epoch, upper, lower,
                        dashes, nul_dashes, bare, z, three, broken, short_bytes, wide, missing, name, inspect, derive };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without OSSP uuid and OpenSSL's libcrypto's headers the program only says what it needs. */
int main(void)
{
    fputs("33-uuid_lib.c needs OSSP uuid and OpenSSL's libcrypto: install its development files, then build with\n"
          "cc 33-uuid_lib.c $(pkg-config --cflags --libs cmetta uuid libcrypto)\n", stderr);
    return 77;
}
#endif
