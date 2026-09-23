/* Purpose: lib_markup, held against libxml2, the C library for XML: a strict
 *   parse, refusing what a recovering parser would repair, and libxml2's own
 *   serializer for writing. C turns libxml2's tree into the same
 *   (element Name Attributes Children) atoms the engine answers and walks
 *   them for the selector language in the order SWI's xpath/3 enumerates,
 *   which the library compiles its selectors to: a descendant step yields the
 *   element itself when named, then each content list's matching elements
 *   before the content of its child elements, and an index counts within one
 *   content list (swipl-devel packages/sgml/xpath.pl, sub_dom/5 and
 *   sub_dom_2/5). An external entity is refused, never fetched. A bare HTML
 *   fragment has no enclosing element for SGML's omitted-end-tag rule to
 *   close into, so SWI nests its unclosed tags there (measured against SWI
 *   10.1.14, which closes the first p inside a div or a body); libxml2's
 *   recovering XML reader nests them the same way, where its HTML parser,
 *   following HTML's tree builder (WHATWG HTML 13.2.6.4.7), would make the two
 *   paragraphs siblings.
 * Assumes: libxml2, found through pkg-config as libxml-2.0.
 * Guarantees: all thirty claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <inttypes.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

enum { MOST = 16 };

/* libxml2's tree as the engine's atoms: an element, its attributes as
   (attr Name Value) rows, its text nodes as texts. */
static mt_atom *atom_of(const xmlNode *node)
{
    mt_atom *attrs[MOST], *kids[MOST];
    size_t na = 0, nk = 0;
    for (const xmlAttr *a = node->properties; a; a = a->next) {
        xmlChar *value = xmlNodeGetContent((const xmlNode *)a);
        require("room for the attributes", na < MOST);
        attrs[na++] = E("attr", S((const char *)a->name), T((const char *)value));
        xmlFree(value);
    }
    for (const xmlNode *c = node->children; c; c = c->next) {
        require("room for the children", nk < MOST);
        if (c->type == XML_ELEMENT_NODE) kids[nk++] = atom_of(c);
        else if (c->type == XML_TEXT_NODE || c->type == XML_CDATA_SECTION_NODE) kids[nk++] = T((const char *)c->content);
    }
    return E("element", S((const char *)node->name), mt_exprv(na, attrs), mt_exprv(nk, kids));
}

/* Whether a document declares an entity libxml2 would have to fetch. */
static bool external_entity(const xmlDoc *doc)
{
    for (const xmlNode *n = doc->intSubset ? doc->intSubset->children : NULL; n; n = n->next)
        if (n->type == XML_ENTITY_DECL && ((const xmlEntity *)n)->etype != XML_INTERNAL_GENERAL_ENTITY) return true;
    return false;
}

/* NULL for text libxml2 refuses under these options, and for a document
   declaring an external entity. */
static mt_atom *parsed(const char *text, int options)
{
    xmlDoc *doc = xmlReadMemory(text, (int)strlen(text), NULL, "UTF-8", options | XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    mt_atom *out = doc && xmlDocGetRootElement(doc) && !external_entity(doc) ? atom_of(xmlDocGetRootElement(doc)) : NULL;
    xmlFreeDoc(doc);
    return out;
}

static mt_atom *strict(const char *text) { return parsed(text, 0); }
static mt_atom *fragment(const char *text) { return parsed(text, XML_PARSE_RECOVER); }

/* The shape the library reads: (element Name ((attr Name Value) ...)
   Children), a Symbol for each name, a text, symbol or number for each
   value, and elements or texts for the children. */
static bool element_ok(const mt_atom *e)
{
    if (mt_kind_of(e) != MT_EXPR || mt_len(e) != 4 || mt_kind_of(mt_at(e, 0)) != MT_SYMBOL || strcmp(mt_name(mt_at(e, 0)), "element") != 0 ||
        mt_kind_of(mt_at(e, 1)) != MT_SYMBOL || mt_kind_of(mt_at(e, 2)) != MT_EXPR || mt_kind_of(mt_at(e, 3)) != MT_EXPR)
        return false;
    for (size_t i = 0; i < mt_len(mt_at(e, 2)); i++) {
        const mt_atom *a = mt_at(mt_at(e, 2), i);
        mt_kind k = mt_len(a) == 3 ? mt_kind_of(mt_at(a, 2)) : MT_NONE;
        if (mt_kind_of(a) != MT_EXPR || mt_len(a) != 3 || mt_kind_of(mt_at(a, 1)) != MT_SYMBOL ||
            !(k == MT_TEXT || k == MT_SYMBOL || k == MT_INT || k == MT_FLOAT))
            return false;
    }
    for (size_t i = 0; i < mt_len(mt_at(e, 3)); i++)
        if (mt_kind_of(mt_at(mt_at(e, 3), i)) != MT_TEXT && !element_ok(mt_at(mt_at(e, 3), i))) return false;
    return true;
}

/* Writing: the atoms back into libxml2's tree, dumped without layout. */
static xmlNode *node_of(const mt_atom *e)
{
    xmlNode *node = xmlNewNode(NULL, (const xmlChar *)mt_name(mt_at(e, 1)));
    const mt_atom *attrs = mt_at(e, 2), *kids = mt_at(e, 3);
    for (size_t i = 0; i < mt_len(attrs); i++) {
        const mt_atom *v = mt_at(mt_at(attrs, i), 2);
        char digits[32];
        if (mt_kind_of(v) == MT_INT) snprintf(digits, sizeof digits, "%" PRId64, mt_int(v));
        xmlNewProp(node, (const xmlChar *)mt_name(mt_at(mt_at(attrs, i), 1)), (const xmlChar *)(mt_kind_of(v) == MT_INT ? digits : mt_name(v)));
    }
    for (size_t i = 0; i < mt_len(kids); i++)
        xmlAddChild(node, mt_kind_of(mt_at(kids, i)) == MT_TEXT ? xmlNewText((const xmlChar *)mt_name(mt_at(kids, i))) : node_of(mt_at(kids, i)));
    return node;
}

/* NULL for what is no element. */
static mt_atom *written(const mt_atom *e)
{
    if (!element_ok(e)) return NULL;
    xmlDoc *doc = xmlNewDoc((const xmlChar *)"1.0");
    xmlNode *root = node_of(e);
    xmlDocSetRootElement(doc, root);
    xmlBuffer *buf = xmlBufferCreate();
    require("libxml2 writes the element", xmlNodeDump(buf, doc, root, 0, 0) >= 0);
    mt_atom *out = T((const char *)xmlBufferContent(buf));
    xmlBufferFree(buf);
    xmlFreeDoc(doc);
    return out;
}

/* Reading an element. */
static bool named(const mt_atom *x, const char *name)
{
    return mt_kind_of(x) == MT_EXPR && mt_len(x) == 4 && strcmp(mt_name(mt_at(x, 1)), name) == 0;
}

static void text_into(const mt_atom *e, char *out, size_t size)
{
    for (size_t i = 0; i < mt_len(mt_at(e, 3)); i++) {
        const mt_atom *c = mt_at(mt_at(e, 3), i);
        if (mt_kind_of(c) == MT_TEXT) strncat(out, mt_name(c), size - strlen(out) - 1);
        else text_into(c, out, size);
    }
}

/* Every text node under the element, joined; NULL for what is no element. */
static mt_atom *text_of(const mt_atom *e)
{
    if (!element_ok(e)) return NULL;
    char out[256] = "";
    text_into(e, out, sizeof out);
    return T(out);
}

static const mt_atom *attribute(const mt_atom *e, const char *name)
{
    for (size_t i = 0; i < mt_len(mt_at(e, 2)); i++)
        if (strcmp(mt_name(mt_at(mt_at(mt_at(e, 2), i), 1)), name) == 0) return mt_at(mt_at(mt_at(e, 2), i), 2);
    return NULL;
}

/* The selector language: a step names elements in one direction, and the
   modifiers after it take the Nth, an attribute or the text. */
typedef enum direction { DESCENDANT, CHILD, SELF } direction;

typedef struct step {
    direction way;
    const char *name;
    int64_t index; /* 0 takes every match */
    const char *attr;
    bool text;
} step;

/* One content list's elements of the step's name, or its Nth. */
static void named_in(const mt_atom *content, const step *s, const mt_atom **out, size_t *n)
{
    int64_t seen = 0;
    for (size_t i = 0; i < mt_len(content); i++)
        if (named(mt_at(content, i), s->name) && (!s->index || ++seen == s->index)) {
            require("room for the matches", *n < MOST);
            out[(*n)++] = mt_at(content, i);
        }
}

/* sub_dom_2: this list's matches, then each child element's content's. */
static void descend(const mt_atom *content, const step *s, const mt_atom **out, size_t *n)
{
    named_in(content, s, out, n);
    for (size_t i = 0; i < mt_len(content); i++)
        if (mt_kind_of(mt_at(content, i)) == MT_EXPR) descend(mt_at(mt_at(content, i), 3), s, out, n);
}

static void matches(const mt_atom *e, const step *s, const mt_atom **out, size_t *n)
{
    if (s->way == SELF || s->way == DESCENDANT) {
        if (named(e, s->name) && s->index <= 1) out[(*n)++] = e;
        if (s->way == SELF) return;
        descend(mt_at(e, 3), s, out, n);
    } else
        named_in(mt_at(e, 3), s, out, n);
}

/* A selector as steps, each modifier folded into the step before it; false
   for an unknown form or a modifier with no step before it. */
static bool steps_of(const mt_atom *selector, step *steps, size_t *n)
{
    static const char *const ways[] = { "descendant", "child", "self" };
    const mt_atom *forms[MOST];
    size_t count = 0;
    *n = 0;
    if (mt_kind_of(selector) != MT_EXPR || !mt_len(selector)) return false;
    if (mt_kind_of(mt_at(selector, 0)) == MT_SYMBOL) forms[count++] = selector;
    else
        for (size_t i = 0; i < mt_len(selector); i++) forms[count++] = mt_at(selector, i);
    for (size_t i = 0; i < count; i++) {
        const mt_atom *f = forms[i];
        if (mt_kind_of(f) != MT_EXPR || !mt_len(f) || mt_kind_of(mt_at(f, 0)) != MT_SYMBOL) return false;
        const char *head = mt_name(mt_at(f, 0));
        bool way = false;
        for (size_t w = 0; w < 3; w++)
            if (strcmp(head, ways[w]) == 0 && mt_len(f) == 2 && mt_kind_of(mt_at(f, 1)) == MT_SYMBOL) {
                steps[(*n)++] = (step){ (direction)w, mt_name(mt_at(f, 1)), 0, NULL, false };
                way = true;
            }
        if (way) continue;
        if (!*n) return false;
        step *last = &steps[*n - 1];
        if (strcmp(head, "text") == 0 && mt_len(f) == 1) last->text = true;
        else if (strcmp(head, "attribute") == 0 && mt_len(f) == 2 && mt_kind_of(mt_at(f, 1)) == MT_SYMBOL) last->attr = mt_name(mt_at(f, 1));
        else if (strcmp(head, "index") == 0 && mt_len(f) == 2 && mt_kind_of(mt_at(f, 1)) == MT_INT && mt_int(mt_at(f, 1)) >= 1)
            last->index = mt_int(mt_at(f, 1));
        else
            return false;
    }
    return *n > 0;
}

/* What a selector answers, in order: each step applied to every element the
   last one found, the final step's modifiers reading the elements it finds.
   NULL for a selector C refuses. */
static mt_atom *selected(const mt_atom *e, const mt_atom *selector)
{
    step steps[MOST];
    size_t n;
    if (!steps_of(selector, steps, &n)) return NULL;
    const mt_atom *context[MOST] = { e }, *next[MOST];
    size_t live = 1;
    for (size_t k = 0; k < n; k++) {
        size_t found = 0;
        for (size_t i = 0; i < live; i++) matches(context[i], &steps[k], next, &found);
        memcpy(context, next, found * sizeof *next);
        live = found;
    }
    mt_atom *out[MOST];
    size_t answers = 0;
    for (size_t i = 0; i < live; i++) {
        const step *last = &steps[n - 1];
        if (last->text) out[answers++] = text_of(context[i]);
        else if (last->attr) {
            const mt_atom *v = attribute(context[i], last->attr);
            if (v) out[answers++] = mt_keep(v);
        } else
            out[answers++] = mt_keep(context[i]);
    }
    return mt_exprv(answers, out);
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

/* The one answer of a list C computed; TAKES the list. */
static mt_atom *only(mt_atom *answers)
{
    require("one answer", mt_len(answers) == 1);
    mt_atom *one = mt_keep(mt_at(answers, 0));
    mt_drop(answers);
    return one;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_markup", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_markup")))));

    /* A document is an expression of elements and texts. */
    const char *order = "<order id='7'><item sku='a'>apple</item><item sku='b'>pear</item><note/></order>";
    mt_atom *tree = strict(order), *doc = mt_one(mt_eval(m, E("markup-parse-xml", T(order))));
    require("both parse the order", tree && doc);
    check_atom("the document C reads", mt_keep(doc), mt_keep(tree));
    mt_atom *rows[MOST];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(mt_at(tree, 3)); i++) {
        const mt_atom *c = mt_at(mt_at(tree, 3), i);
        if (named(c, "item") && mt_len(mt_at(c, 2)) == 1 && mt_len(mt_at(c, 3)) == 1) rows[n++] = E(mt_keep(attribute(c, "sku")), mt_keep(mt_at(mt_at(c, 3), 0)));
    }
    check_answers("read by its shape",
                  mt_eval(m, E("collapse", E("let", V("children"), E("index-atom", mt_keep(doc), 3),
                                             E("let", E("element", "item", E(E("attr", "sku", V("sku"))), E(V("text"))), E("superpose", V("children")),
                                               E(V("sku"), V("text")))))),
                  mt_exprv(n, rows));

    /* The readers. */
    check_answers("markup-text", mt_eval(m, E("markup-text", mt_keep(doc))), text_of(tree));
    check_answers("markup-attribute", mt_eval(m, E("markup-attribute", mt_keep(doc), "id")), mt_keep(attribute(tree, "id")));
    require("no such attribute in C's tree", attribute(tree, "missing") == NULL);
    check_none("an absent attribute has no answer", mt_eval(m, E("markup-attribute", mt_keep(doc), "missing")));
    mt_atom *empty_p = E("element", "p", mt_unit(), mt_unit());
    check_answers("an empty element's text", mt_eval(m, E("markup-text", mt_keep(empty_p))), text_of(empty_p));

    /* Selectors. */
    mt_atom *selectors[] = {
        E(E("descendant", "item"), E("text")),
        E(E("descendant", "item"), E("attribute", "sku")),
        E("child", "item"),
        E("self", "order"),
        E("descendant", "missing"),
    };
    const char *claims[] = { "texts of every item", "an attribute of every item", "children", "the element itself", "nothing" };
    for (size_t i = 0; i < sizeof selectors / sizeof *selectors; i++) {
        check_answers(claims[i], mt_eval(m, E("collapse", E("markup-select", mt_keep(doc), mt_keep(selectors[i])))), selected(tree, selectors[i]));
        mt_drop(selectors[i]);
    }
    mt_atom *second = E(E("descendant", "item"), E("index", 2), E("text"));
    check_answers("the second item's text", mt_eval(m, E("markup-select", mt_keep(doc), mt_keep(second))), only(selected(tree, second)));
    const char *deep = "<a><b><c>deep</c></b><c>shallow</c></a>";
    mt_atom *nest = strict(deep), *nested = mt_one(mt_eval(m, E("markup-parse-xml", T(deep))));
    require("both parse the nesting", nest && nested);
    mt_atom *path = E(E("child", "b"), E("child", "c"), E("text")), *all_c = E(E("descendant", "c"), E("text"));
    check_answers("a path of children", mt_eval(m, E("collapse", E("markup-select", mt_keep(nested), mt_keep(path)))), selected(nest, path));
    check_answers("a level's matches before the next level's", mt_eval(m, E("collapse", E("markup-select", mt_keep(nested), mt_keep(all_c)))),
                  selected(nest, all_c));

    /* Writing is libxml2's serializer. */
    mt_atom *item = E("element", "item", E(E("attr", "sku", T("a"))), E(T("apple"))), *empty = E("element", "empty", mt_unit(), mt_unit()),
            *values = E("element", "a", E(E("attr", "n", 1), E("attr", "k", "sym")), mt_unit());
    check_answers("markup-write", mt_eval(m, E("markup-write", mt_keep(item))), written(item));
    mt_atom *text = written(tree);
    check_answers("which parses back", mt_eval(m, E("markup-parse-xml", E("markup-write", mt_keep(doc)))), strict(mt_name(text)));
    check_answers("an empty element", mt_eval(m, E("markup-write", mt_keep(empty))), written(empty));
    check_answers("numbers and symbols are text", mt_eval(m, E("markup-write", mt_keep(values))), written(values));

    /* A bare HTML fragment. */
    check_answers("unclosed tags nest in a fragment", mt_eval(m, E("markup-parse-html", T("<p>one<p>two"))), fragment("<p>one<p>two"));
    mt_atom *inline_b = fragment("<p>a<b>c</b></p>");
    check_answers("its text", mt_eval(m, E("markup-text", E("markup-parse-html", T("<p>a<b>c</b></p>")))), text_of(inline_b));

    /* Strict: a parse refuses what libxml2 refuses without recovery. */
    static const char *const broken[][2] = {
        { "a missing end tag", "<a><b></a>" },
        { "an unclosed element", "<a>" },
        { "no markup at all", "not markup at all" },
        { "nothing", "" },
        { "an external entity", "<!DOCTYPE d [<!ENTITY e SYSTEM '/etc/passwd'>]><d>&e;</d>" },
    };
    for (size_t i = 0; i < sizeof broken / sizeof *broken; i++)
        check_answers(broken[i][0], mt_eval(m, E("if-error", E("catch", E("markup-parse-xml", T(broken[i][1]))), "refused", "fine")),
                      verdict(computed(strict(broken[i][1]))));

    /* Selectors and elements C cannot read are refused. */
    mt_atom *bad[] = { E("nosuch", "item"), E(E("text"), E("descendant", "item")), E("index", 1) };
    const char *why[] = { "an unknown selector", "a modifier before any step", "a modifier alone" };
    for (size_t i = 0; i < 3; i++) {
        check_answers(why[i], mt_eval(m, guarded(E("markup-select", mt_keep(doc), mt_keep(bad[i])))), verdict(computed(selected(tree, bad[i]))));
        mt_drop(bad[i]);
    }
    mt_atom *seven = mt_num(7), *nosuch = E("nosuch");
    check_answers("a number is no element", mt_eval(m, guarded(E("markup-text", mt_keep(seven)))), verdict(computed(text_of(seven))));
    check_answers("nor is (nosuch)", mt_eval(m, guarded(E("markup-write", mt_keep(nosuch)))), verdict(computed(written(nosuch))));

    mt_atom *held[] = { tree, doc, empty_p, second, nest, nested, path, all_c, item, empty, values, text, inline_b, seven, nosuch };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
