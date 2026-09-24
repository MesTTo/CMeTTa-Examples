/* Purpose: lib_uuid, held against libuuid, util-linux's C library for UUIDs:
 *   uuid_generate_random and uuid_generate_time for versions 4 and 1,
 *   uuid_generate_md5 and uuid_generate_sha1 over uuid_get_template's
 *   namespaces for versions 3 and 5, uuid_parse and uuid_unparse_lower for
 *   the text, and uuid_type, uuid_variant and uuid_time for the fields. A
 *   name is its bytes with their length, so a NUL inside one counts. C
 *   refuses what libuuid refuses: text uuid_parse will not read, a namespace
 *   uuid_get_template has no template for, and a version with no name-based
 *   generator; and bytes that are not sixteen numbers from 0 to 255.
 * Assumes: libuuid and libcrypto (for the shared hex), through pkg-config.
 * Guarantees: all forty-nine claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <uuid.h>
#include "crypto_oracle.h"

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
    metta *m = open_engine();
    static const char *const libraries[] = { "lib_crypto", "lib_uuid", "lib_encoding", "lib_string" };
    for (size_t i = 0; i < 4; i++) require("import", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));

    /* Generated identifiers, beside C's own. */
    uuid_t random, again, timed, nil;
    uuid_generate_random(random);
    uuid_generate_random(again);
    uuid_generate_time(timed);
    uuid_clear(nil);
    double when;
    check_answers("a UUID is sixteen bytes", mt_eval(m, E("size-atom", E("crypto-random-bytes", 16))), (int64_t)sizeof(uuid_t));
    check_answers("random is version 4", mt_eval(m, E("uuid-version", E("uuid-random!"))), (int64_t)uuid_type(random));
    check_answers("of the RFC variant", mt_eval(m, E("uuid-variant", E("uuid-random!"))), S(variant_name(random)));
    mt_atom *random_text = text_of(random);
    uuid_t scratch;
    check_answers("a UUID", mt_eval(m, E("uuid-is", E("uuid-random!"))), B(parsed(random_text, scratch)));
    check_answers("two draws differ", mt_eval(m, E("!=", E("uuid-random!"), E("uuid-random!"))), B(uuid_compare(random, again) != 0));
    check_answers("time-based is version 1", mt_eval(m, E("uuid-version", E("uuid-time!"))), (int64_t)uuid_type(timed));
    check_answers("of the RFC variant too", mt_eval(m, E("uuid-variant", E("uuid-time!"))), S(variant_name(timed)));
    check_answers("with a time", mt_eval(m, E(">", E("uuid-timestamp", E("uuid-time!")), 0)), B(timestamp(timed, &when) && when > 0));

    /* Names are deterministic: libuuid's generators over its templates. */
    static const char *const templates[] = { "dns", "url", "oid", "x500" };
    mt_atom *spaces[4];
    for (size_t i = 0; i < 4; i++) {
        uuid_t t;
        mt_atom *name = S(templates[i]);
        require("libuuid has the template", namespace_of(name, t));
        spaces[i] = name;
    }
    check_answers("the namespaces", mt_eval(m, E("uuid-namespaces")), mt_exprv(4, spaces));
    static const struct {
        int version;
        const char *ns, *name;
    } names[] = {
        { 3, "dns", "example.com" }, { 5, "dns", "example.com" }, { 3, "url", "http://example.com" }, { 5, "oid", "1.3.6.1.4.1" },
        { 5, "x500", "cn=Test" },    { 5, "dns", "" },             { 5, "dns", "é" },                  { 3, "dns", "漢" },
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        mt_atom *ns = S(names[i].ns), *name = T(names[i].name);
        char claim[64];
        snprintf(claim, sizeof claim, "version %d in %s", names[i].version, names[i].ns);
        check_answers(claim, mt_eval(m, E("uuid-name", names[i].version, mt_keep(ns), mt_keep(name))), named(names[i].version, ns, name));
        mt_drop(ns);
        mt_drop(name);
    }
    mt_atom *dns_text = T("6ba7b810-9dad-11d1-80b4-00c04fd430c8"), *dns = S("dns"), *example = T("example.com");
    mt_atom *by_text = named(5, dns_text, example), *by_name = named(5, dns, example);
    check_answers("a namespace is also its own text",
                  mt_eval(m, E("==", E("uuid-name", 5, mt_keep(dns_text), mt_keep(example)), E("uuid-name", 5, mt_keep(dns), mt_keep(example)))),
                  B(mt_eq(by_text, by_name)));
    static const char nul_name[] = { 'a', '\0', 'b' };
    mt_atom *with_nul = mt_textn(nul_name, 3), *nul_codes = E(97, 0, 98);
    check_answers("a NUL is part of a name", mt_eval(m, E("uuid-name", 5, mt_keep(dns), E("string-from-codes", mt_keep(nul_codes)))),
                  named(5, dns, with_nul));

    /* The nil UUID and the variants. */
    check_answers("nil", mt_eval(m, E("uuid-nil")), text_of(nil));
    mt_atom *nil_text = text_of(nil);
    check_answers("is a UUID", mt_eval(m, E("uuid-is", E("uuid-nil"))), B(parsed(nil_text, scratch)));
    check_answers("of version 0", mt_eval(m, E("uuid-version", E("uuid-nil"))), (int64_t)uuid_type(nil));
    check_answers("and the NCS variant", mt_eval(m, E("uuid-variant", E("uuid-nil"))), S(variant_name(nil)));
    static const char *const variants[] = { "00000000-0000-0000-c000-000000000000", "ffffffff-ffff-ffff-ffff-ffffffffffff" };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *text = T(variants[i]);
        uuid_t u;
        require("libuuid reads it", parsed(text, u));
        check_answers(variants[i], mt_eval(m, E("uuid-variant", mt_keep(text))), S(variant_name(u)));
        mt_drop(text);
    }
    mt_atom *epoch = T("13814000-1dd2-11b2-8000-000000000000");
    uuid_t epoch_u;
    require("libuuid reads the epoch", parsed(epoch, epoch_u) && timestamp(epoch_u, &when));
    check_answers("version 1's epoch", mt_eval(m, E("uuid-timestamp", mt_keep(epoch))), when);
    require("no time in other versions", !timestamp(random, &when) && !timestamp(nil, &when));
    check_none("a random UUID has no time", mt_eval(m, E("uuid-timestamp", E("uuid-random!"))));
    check_none("nor has nil", mt_eval(m, E("uuid-timestamp", E("uuid-nil"))));

    /* Text either case in, lower case out; all 128 bits as bytes. */
    mt_atom *upper = T("CFBFF0D1-9375-5685-968C-48CE8B15AE17"), *lower = T("cfbff0d1-9375-5685-968c-48ce8b15ae17");
    uuid_t u, all_ones;
    require("libuuid reads both cases", parsed(upper, u));
    check_answers("upper case reads", mt_eval(m, E("uuid-is", mt_keep(upper))), B(parsed(upper, scratch)));
    check_answers("the bytes, in hex", mt_eval(m, E("hex-encode", E("uuid-bytes", mt_keep(lower)))), hex(u, 16));
    check_answers("and back to lower case", mt_eval(m, E("uuid-of-bytes", E("uuid-bytes", mt_keep(upper)))), text_of(u));
    check_answers("nil's bytes", mt_eval(m, E("uuid-bytes", E("uuid-nil"))), mt_array(sizeof(uuid_t), nil));
    memset(all_ones, 0xff, sizeof all_ones);
    check_answers("every bit set", mt_eval(m, E("uuid-of-bytes", E("hex-decode", T("ffffffffffffffffffffffffffffffff")))), text_of(all_ones));

    /* What is not a UUID. */
    mt_atom *dashes = T("------------------------------------"), *nul_dashes = NULL, *bare = T("cfbff0d193755685968c48ce8b15ae17"),
            *z = T("zfbff0d1-9375-5685-968c-48ce8b15ae17"), *three = mt_num(3);
    char zeros[37];
    snprintf(zeros, sizeof zeros, "%s", mt_name(nil_text));
    for (char *c = zeros; *c; c++)
        if (*c == '-') *c = '\0';
    nul_dashes = mt_textn(zeros, 36);
    check_answers("dashes alone", mt_eval(m, E("uuid-is", mt_keep(dashes))), B(parsed(dashes, scratch)));
    check_answers("NULs for dashes", mt_eval(m, E("uuid-is", E("string-replace", E("uuid-nil"), T("-"), E("string-from-codes", E(0))))),
                  B(parsed(nul_dashes, scratch)));
    check_answers("no dashes", mt_eval(m, E("uuid-is", mt_keep(bare))), B(parsed(bare, scratch)));
    check_answers("a letter past f", mt_eval(m, E("uuid-is", mt_keep(z))), B(parsed(z, scratch)));
    check_answers("a number", mt_eval(m, E("uuid-is", mt_keep(three))), B(parsed(three, scratch)));
    mt_atom *broken = T("broken"), *short_bytes = E(1, 2), *wide = E(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 256), *missing = S("missing"),
            *name = T("name");
    check_answers("no version of broken text", mt_eval(m, guarded(E("uuid-version", mt_keep(broken)))), verdict(parsed(broken, scratch)));
    check_answers("no time of it", mt_eval(m, guarded(E("uuid-timestamp", mt_keep(broken)))), verdict(parsed(broken, scratch)));
    check_answers("two bytes", mt_eval(m, guarded(E("uuid-of-bytes", mt_keep(short_bytes)))), verdict(from_bytes(short_bytes, scratch)));
    check_answers("a byte of 256", mt_eval(m, guarded(E("uuid-of-bytes", mt_keep(wide)))), verdict(from_bytes(wide, scratch)));
    check_answers("no name-based version 2", mt_eval(m, guarded(E("uuid-name", 2, mt_keep(dns), mt_keep(name)))), verdict(computed(named(2, dns, name))));
    check_answers("no such namespace", mt_eval(m, guarded(E("uuid-name", 5, mt_keep(missing), mt_keep(name)))), verdict(computed(named(5, missing, name))));
    check_answers("no namespace in broken text", mt_eval(m, guarded(E("uuid-name", 5, mt_keep(broken), mt_keep(name)))),
                  verdict(computed(named(5, broken, name))));

    /* The equations read back out of &self. */
    const char *id[] = { "id" }, *three_params[] = { "v", "ns", "name" };
    mt_atom *inspect = recipe(m, E("uuid-version", V("id")), id, 1);
    check_answers("a version recipe", mt_eval(m, E(inspect, E("uuid-nil"))), (int64_t)uuid_type(nil));
    mt_atom *derive = recipe(m, E("uuid-name", V("v"), V("ns"), V("name")), three_params, 3);
    check_answers("a name recipe", mt_eval(m, E(derive, 5, "dns", mt_keep(example))), named(5, dns, example));
    check_answers("one version per alternative",
                  mt_eval(m, E("collapse", E("uuid-version", E("superpose", E(T("00000000-0000-0000-0000-000000000000"), T("ffffffff-ffff-ffff-ffff-ffffffffffff")))))),
                  E((int64_t)uuid_type(nil), (int64_t)uuid_type(all_ones)));
    check_answers("a NUL in a version 3 name", mt_eval(m, E("uuid-name", 3, mt_keep(dns), E("string-from-codes", mt_keep(nul_codes)))),
                  named(3, dns, with_nul));

    mt_atom *held[] = { random_text, dns_text, dns, example, by_text, by_name, with_nul, nul_codes, nil_text, epoch, upper, lower,
                        dashes, nul_dashes, bare, z, three, broken, short_bytes, wide, missing, name, inspect, derive };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
