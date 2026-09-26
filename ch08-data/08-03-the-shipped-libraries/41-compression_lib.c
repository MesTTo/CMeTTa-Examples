/* Purpose: lib_compression, held against zlib and libarchive, the C
 *   libraries it wraps, and against C reading what the engine wrote. C
 *   inflates the engine's gzip and zlib bytes and files itself, and runs the
 *   same file program in a directory of its own: staging beside the
 *   destination and publishing with rename(2), so a failure leaves the old
 *   file. Archives are read by libarchive from a seekable file, a gzip layer
 *   decoded first as the library decodes it, because libarchive's streaming
 *   ZIP reader has no central directory and reports other permissions. Names
 *   follow PKWARE's APPNOTE, read from the central directory by C: bit 11
 *   means UTF-8, else a Unicode Path extra field whose CRC matches the
 *   header's bytes, else CP437 through iconv; stock libarchive given CP437
 *   checks that CRC after converting and drops a valid field. Extraction
 *   refuses the names the library refuses before anything is published.
 * Build: cc 41-compression_lib.c $(pkg-config --cflags --libs cmetta zlib
 *   libarchive)
 * Guarantees: all fifty-four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#if __has_include(<zlib.h>) && __has_include(<archive.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <iconv.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

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

/* A growable byte string. */
typedef struct bytes {
    unsigned char *at;
    size_t n, cap;
} bytes;

static void put(bytes *b, const void *data, size_t n)
{
    if (b->n + n > b->cap) {
        b->cap = 2 * (b->n + n) + 16;
        b->at = realloc(b->at, b->cap);
        require("room for the bytes", b->at != NULL);
    }
    if (n) memcpy(b->at + b->n, data, n);
    b->n += n;
}

static mt_atom *taken(bytes *b)
{
    mt_atom *out = mt_array(b->n, b->at);
    free(b->at);
    *b = (bytes){ 0 };
    return out;
}

/* The envelopes and zlib's window bits for each [source: zlib.h,
   deflateInit2, "windowBits can also be greater than 15 for optional gzip
   encoding"]. */
static const struct {
    const char *name;
    int window;
} envelopes[] = { { "gzip", 15 + 16 }, { "zlib", 15 } };
enum { ENVELOPES = sizeof envelopes / sizeof *envelopes };

static int envelope_of(const char *name)
{
    for (int i = 0; i < ENVELOPES; i++)
        if (strcmp(envelopes[i].name, name) == 0) return envelopes[i].window;
    return 0;
}

/* One complete member at `level`; false for an unknown envelope or a level
   outside 0 to 9, which zlib's -1 default is too. */
static bool compressed(const char *format, int64_t level, const unsigned char *in, size_t n, bytes *out)
{
    int window = envelope_of(format);
    z_stream z = { 0 };
    unsigned char chunk[4096];
    int rc;
    *out = (bytes){ 0 };
    if (!window || level < 0 || level > 9) return false;
    require("deflateInit2", deflateInit2(&z, (int)level, Z_DEFLATED, window, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    z.next_in = (Bytef *)in;
    z.avail_in = (uInt)n;
    do {
        z.next_out = chunk;
        z.avail_out = sizeof chunk;
        rc = deflate(&z, Z_FINISH);
        put(out, chunk, sizeof chunk - z.avail_out);
    } while (rc == Z_OK);
    deflateEnd(&z);
    return rc == Z_STREAM_END;
}

/* Bytes given as integers, each a byte or refused. */
static bool byte_values(const int64_t *values, size_t n, unsigned char *out)
{
    for (size_t i = 0; i < n; i++) {
        if (values[i] < 0 || values[i] > 255) return false;
        out[i] = (unsigned char)values[i];
    }
    return true;
}

/* Every complete member decoded and concatenated; false for no input, the
   other envelope, a truncated member, a checksum error or trailing bytes
   that are no member. */
static bool decompressed(const char *format, const unsigned char *in, size_t n, bytes *out)
{
    int window = envelope_of(format);
    z_stream z = { 0 };
    unsigned char chunk[4096];
    int rc = Z_OK;
    *out = (bytes){ 0 };
    if (!window || n == 0) return false;
    require("inflateInit2", inflateInit2(&z, window) == Z_OK);
    z.next_in = (Bytef *)in;
    z.avail_in = (uInt)n;
    while (z.avail_in) {
        do {
            z.next_out = chunk;
            z.avail_out = sizeof chunk;
            rc = inflate(&z, Z_NO_FLUSH);
            put(out, chunk, sizeof chunk - z.avail_out);
        } while (rc == Z_OK && (z.avail_in || !z.avail_out));
        if (rc != Z_STREAM_END) break;
        if (z.avail_in) inflateReset(&z);
    }
    inflateEnd(&z);
    return rc == Z_STREAM_END;
}

static bool read_file(const char *path, bytes *out)
{
    FILE *f = fopen(path, "rb");
    unsigned char chunk[4096];
    size_t got;
    *out = (bytes){ 0 };
    if (!f) return false;
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) put(out, chunk, got);
    return fclose(f) == 0;
}

/* Bytes written to a staging file beside `path`, then renamed over it, so
   the old file stays whole until the new one is complete. */
static bool published(const char *path, const bytes *data)
{
    char staging[PATH_MAX], directory[PATH_MAX];
    snprintf(directory, sizeof directory, "%s", path);
    snprintf(staging, sizeof staging, "%s/.staging-XXXXXX", dirname(directory));
    int fd = mkstemp(staging);
    if (fd < 0) return false;
    bool whole = write(fd, data->at ? (const void *)data->at : "", data->n) == (ssize_t)data->n;
    whole = close(fd) == 0 && whole;
    if (whole && rename(staging, path) == 0) return true;
    unlink(staging);
    return false;
}

static bool compressed_file(const char *format, int64_t level, const char *source, const char *destination)
{
    bytes in, out;
    bool fine = read_file(source, &in) && compressed(format, level, in.at, in.n, &out) && published(destination, &out);
    free(in.at);
    free(out.at);
    return fine;
}

static bool decompressed_file(const char *format, const char *source, const char *destination)
{
    bytes in, out = { 0 };
    bool fine = read_file(source, &in) && decompressed(format, in.at, in.n, &out) && published(destination, &out);
    free(in.at);
    free(out.at);
    return fine;
}

/* An archive as libarchive reads it seekably: a gzip layer, told by its
   magic bytes, is decoded to a scratch file first. */
static struct archive *opened(const char *path, const char *scratch)
{
    bytes raw, plain;
    const char *reading = path;
    if (read_file(path, &raw) && raw.n >= 2 && raw.at[0] == 0x1f && raw.at[1] == 0x8b) {
        require("the gzip layer decodes", decompressed("gzip", raw.at, raw.n, &plain) && published(scratch, &plain));
        free(plain.at);
        reading = scratch;
    }
    free(raw.at);
    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    require("libarchive opens the archive", archive_read_open_filename(a, reading, 10240) == ARCHIVE_OK);
    return a;
}

/* An entry as libarchive describes it. */
typedef struct entry {
    char *name, *format;
    mode_t type, perm;
    int64_t size, mtime;
} entry;

static size_t entries_of(const char *path, const char *scratch, entry **out)
{
    struct archive *a = opened(path, scratch);
    struct archive_entry *e;
    size_t n = 0, cap = 4;
    *out = malloc(cap * sizeof **out);
    require("room for the entries", *out != NULL);
    while (archive_read_next_header(a, &e) == ARCHIVE_OK) {
        if (n == cap) *out = realloc(*out, (cap *= 2) * sizeof **out), require("room for the entries", *out != NULL);
        (*out)[n++] = (entry){ strdup(archive_entry_pathname(e)), strdup(archive_format_name(a)), archive_entry_filetype(e),
                               archive_entry_perm(e), archive_entry_size(e), archive_entry_mtime(e) };
        archive_read_data_skip(a);
    }
    archive_read_free(a);
    return n;
}

static void forget_entries(entry *es, size_t n)
{
    for (size_t i = 0; i < n; i++) free(es[i].name), free(es[i].format);
    free(es);
}

static bool same_entries(const entry *a, size_t an, const entry *b, size_t bn)
{
    bool same = an == bn;
    for (size_t i = 0; same && i < an; i++)
        same = strcmp(a[i].name, b[i].name) == 0 && strcmp(a[i].format, b[i].format) == 0 && a[i].type == b[i].type &&
               a[i].perm == b[i].perm && a[i].size == b[i].size && a[i].mtime == b[i].mtime;
    return same;
}

/* One regular entry's bytes by ordinal; false when it is absent or no
   regular file. */
static bool entry_bytes(const char *path, const char *scratch, size_t index, bytes *out)
{
    struct archive *a = opened(path, scratch);
    struct archive_entry *e;
    unsigned char chunk[4096];
    bool found = false;
    ssize_t got;
    *out = (bytes){ 0 };
    for (size_t i = 0; archive_read_next_header(a, &e) == ARCHIVE_OK; i++)
        if (i == index && archive_entry_filetype(e) == AE_IFREG) {
            while ((got = archive_read_data(a, chunk, sizeof chunk)) > 0) put(out, chunk, (size_t)got);
            found = got == 0;
        } else
            archive_read_data_skip(a);
    archive_read_free(a);
    return found;
}

static uint32_t le(const unsigned char *p, int n)
{
    uint32_t v = 0;
    for (int i = n; i-- > 0;) v = v << 8 | p[i];
    return v;
}

/* CP437's bytes as UTF-8, through iconv. */
static char *from_cp437(const char *raw, size_t n)
{
    iconv_t cd = iconv_open("UTF-8", "CP437");
    size_t room = 4 * n + 1, left = room;
    char *out = malloc(room), *to = out, *from = (char *)raw;
    require("iconv knows CP437", cd != (iconv_t)-1 && out != NULL && iconv(cd, &from, &n, &to, &left) != (size_t)-1);
    *to = '\0';
    iconv_close(cd);
    return out;
}

/* Each entry's name as PKWARE's APPNOTE decides it, from the central
   directory: general-purpose bit 11 means UTF-8; else a version-1 Info-ZIP
   Unicode Path field whose CRC-32 matches the name's own bytes names it in
   UTF-8; else the bytes are CP437 [source: PKWARE APPNOTE.TXT 6.3.10,
   sections 4.3.12, 4.3.16, 4.4.4 and 4.6.9, appendix D]. */
static size_t zip_names(const char *path, char ***names)
{
    bytes zip;
    require("the archive reads", read_file(path, &zip) && zip.n >= 22);
    size_t end = zip.n - 22;
    while (end > 0 && le(zip.at + end, 4) != 0x06054b50) end--;
    size_t count = le(zip.at + end + 10, 2), at = le(zip.at + end + 16, 4);
    *names = malloc((count + 1) * sizeof **names);
    require("the end record and room for the names", le(zip.at + end, 4) == 0x06054b50 && *names != NULL);
    for (size_t i = 0; i < count; i++) {
        const unsigned char *h = zip.at + at;
        require("a central header", le(h, 4) == 0x02014b50);
        uint32_t flags = le(h + 8, 2), name_n = le(h + 28, 2), extra_n = le(h + 30, 2), comment_n = le(h + 32, 2);
        const char *name = (const char *)h + 46;
        const unsigned char *extra = h + 46 + name_n;
        char *chosen = NULL;
        for (uint32_t x = 0; !(flags & 0x800) && !chosen && x + 4 <= extra_n; x += 4 + le(extra + x + 2, 2)) {
            uint32_t id = le(extra + x, 2), size = le(extra + x + 2, 2);
            const unsigned char *field = extra + x + 4;
            if (id == 0x7075 && size >= 5 && field[0] == 1 && le(field + 1, 4) == crc32(0, (const Bytef *)name, name_n))
                chosen = strndup((const char *)field + 5, size - 5);
        }
        (*names)[i] = flags & 0x800 ? strndup(name, name_n) : chosen ? chosen : from_cp437(name, name_n);
        at += 46 + name_n + extra_n + comment_n;
    }
    free(zip.at);
    return count;
}

static char *first_name(const char *path)
{
    char **names;
    size_t n = zip_names(path, &names);
    require("an entry", n > 0);
    for (size_t i = 1; i < n; i++) free(names[i]);
    char *first = names[0];
    free(names);
    return first;
}

/* A component a portable relative path may hold: not a parent, no leading
   space, no trailing space or dot, and no Windows device name, whatever the
   extension [source: lib/lib_compression/lib_compression.pl,
   portable_component/1, which cites Microsoft's file naming conventions]. */
static bool portable_component(const char *part, size_t n)
{
    static const char *const devices[] = { "CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$", "CLOCK$" };
    char stem[16];
    size_t k = 0, s = 0;
    if ((n == 2 && memcmp(part, "..", 2) == 0) || part[0] == ' ' || part[n - 1] == ' ' || part[n - 1] == '.') return false;
    while (s < n && part[s] == ' ') s++;
    while (s < n && part[s] != '.' && k + 1 < sizeof stem) stem[k++] = (char)toupper((unsigned char)part[s++]);
    while (k && stem[k - 1] == ' ') k--;
    stem[k] = '\0';
    for (size_t i = 0; i < sizeof devices / sizeof *devices; i++)
        if (strcmp(stem, devices[i]) == 0) return false;
    bool numbered = k >= 4 && (memcmp(stem, "COM", 3) == 0 || memcmp(stem, "LPT", 3) == 0);
    if (numbered && k == 4 && stem[3] >= '1' && stem[3] <= '9') return false;
    if (numbered && k == 5 && strcmp(stem + 3, "\xc2\xb9") == 0) return false;
    if (numbered && (strcmp(stem + 3, "\xc2\xb2") == 0 || strcmp(stem + 3, "\xc2\xb3") == 0)) return false;
    return true;
}

/* A portable relative path, with empty and . components dropped; false for
   an absolute path, a control character or reserved punctuation, or a bad
   component; the parts land in `clean`. */
static bool portable(const char *name, char *clean, size_t room)
{
    size_t used = 0;
    if (!*name || name[0] == '/') return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (*p < 32 || strchr("\"*:<>?\\|", *p)) return false;
    clean[0] = '\0';
    for (const char *p = name; *p;) {
        size_t n = strcspn(p, "/");
        if (n && !(n == 1 && p[0] == '.')) {
            if (!portable_component(p, n) || used + n + 2 > room) return false;
            if (used) clean[used++] = '/';
            memcpy(clean + used, p, n);
            clean[used += n] = '\0';
        }
        p += n + (p[n] == '/');
    }
    return true;
}

static int unlink_one(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

static bool removed_tree(const char *path) { return nftw(path, unlink_one, 16, FTW_DEPTH | FTW_PHYS) == 0; }

/* A destination extraction may publish into: missing, or an empty
   directory. */
static bool empty_or_missing(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *e;
    bool empty = true;
    if (!d) return errno == ENOENT;
    while (empty && (e = readdir(d)))
        empty = strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0;
    closedir(d);
    return empty;
}

static bool made_parents(char *path)
{
    for (char *slash = strchr(path + 1, '/'); slash; slash = strchr(slash + 1, '/')) {
        *slash = '\0';
        bool fine = mkdir(path, 0777) == 0 || errno == EEXIST;
        *slash = '/';
        if (!fine) return false;
    }
    return true;
}

/* Regular files and directories built in a staging tree beside the
   destination, which must be missing or empty, then renamed into place; a
   refused name, another kind of entry or a duplicate file publishes
   nothing. */
static bool extracted(const char *path, const char *scratch, const char *destination)
{
    char staging[PATH_MAX], parent[PATH_MAX], clean[PATH_MAX], target[2 * PATH_MAX];
    struct archive *a;
    struct archive_entry *e;
    unsigned char chunk[4096];
    bool fine = true;
    ssize_t got;
    snprintf(parent, sizeof parent, "%s", destination);
    snprintf(staging, sizeof staging, "%s/.extracting-XXXXXX", dirname(parent));
    if (!empty_or_missing(destination) || !mkdtemp(staging)) return false;
    a = opened(path, scratch);
    while (fine && archive_read_next_header(a, &e) == ARCHIVE_OK) {
        mode_t type = archive_entry_filetype(e);
        fine = (type == AE_IFREG || type == AE_IFDIR) && portable(archive_entry_pathname(e), clean, sizeof clean) && (*clean || type == AE_IFDIR);
        snprintf(target, sizeof target, "%s/%s", staging, clean);
        if (fine && type == AE_IFDIR) fine = !*clean || ((made_parents(target) && (mkdir(target, 0777) == 0 || errno == EEXIST)));
        else if (fine) {
            int fd = made_parents(target) ? open(target, O_WRONLY | O_CREAT | O_EXCL, 0666) : -1;
            fine = fd >= 0;
            while (fine && (got = archive_read_data(a, chunk, sizeof chunk)) > 0) fine = write(fd, chunk, (size_t)got) == got;
            if (fd >= 0) fine = close(fd) == 0 && fine;
        }
    }
    archive_read_free(a);
    struct stat st;
    if (fine && stat(destination, &st) == 0) fine = rmdir(destination) == 0;
    if (fine && rename(staging, destination) == 0) return true;
    removed_tree(staging);
    return false;
}

static bool exists_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool exists_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* A byte list the engine answered, as C bytes. */
static bytes bytes_from(const mt_atom *list)
{
    bytes b = { 0 };
    for (size_t i = 0; i < mt_len(list); i++) {
        unsigned char byte = (unsigned char)mt_int(mt_at(list, i));
        put(&b, &byte, 1);
    }
    return b;
}

/* The bytes C inflates from the engine's compressed list. */
static mt_atom *inflated(const char *format, const mt_atom *list)
{
    bytes in = bytes_from(list), out;
    require("C inflates the engine's bytes", decompressed(format, in.at, in.n, &out));
    free(in.at);
    return taken(&out);
}

static mt_atom *file_bytes(const char *path)
{
    bytes b;
    require("C reads the file", read_file(path, &b));
    return taken(&b);
}

static bool computed(bool fine, bytes *b)
{
    free(b->at);
    *b = (bytes){ 0 };
    return fine;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

static const char *in_dir(const char *dir, const char *name)
{
    static char paths[16][PATH_MAX];
    static unsigned next;
    char *out = paths[next++ % 16];
    snprintf(out, PATH_MAX, "%s/%s", dir, name);
    return out;
}

int main(void)
{
    locale_t utf8 = newlocale(LC_CTYPE_MASK, "C.UTF-8", (locale_t)0);
    require("a UTF-8 character locale", utf8 != (locale_t)0 && uselocale(utf8) != (locale_t)0);
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    require("import lib_compression", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_compression")))));

    mt_atom *names[ENVELOPES];
    for (int i = 0; i < ENVELOPES; i++) names[i] = S(envelopes[i].name);
    assert(answers_are(mt_eval(m, E("compression-formats")), E(mt_exprv(ENVELOPES, names))) && "the envelopes");
    assert(answers_are(mt_eval(m, E("path-join", T("one"), T("two"))), E(T(in_dir("one", "two")))) && "a path join");

    /* Bytes, compressed by the engine and inflated by C, and the reverse. */
    static const unsigned char data[] = { 0, 128, 255, 10 };
    mt_atom *data_list = mt_array(4, data);
    mt_atom *gzipped = value_of(m, E("compress-bytes", "gzip", 6, mt_keep(data_list)));
    mt_atom *zlibbed = value_of(m, E("compress-bytes", "zlib", 6, mt_keep(data_list)));
    bytes own;
    require("C gzips", compressed("gzip", 6, data, 4, &own));
    assert(answers_are(mt_eval(m, E("car-atom", mt_keep(gzipped))), E(mt_num(own.at[0]))) && "gzip's first magic byte");
    assert(answers_are(mt_eval(m, E("car-atom", E("cdr-atom", mt_keep(gzipped)))), E(mt_num(own.at[1]))) && "and its second");
    free(own.at);
    assert(answers_are(mt_eval(m, E("decompress-bytes", "gzip", mt_keep(gzipped))), E(inflated("gzip", gzipped))) && "gzip decodes");
    assert(answers_are(mt_eval(m, E("decompress-bytes", "zlib", mt_keep(zlibbed))), E(inflated("zlib", zlibbed))) && "zlib decodes");
    static const struct {
        const char *format;
        int64_t level;
        unsigned char data[5];
        size_t n;
    } rounds[] = { { "gzip", 0, { 0 }, 0 }, { "zlib", 9, { 0 }, 0 }, { "gzip", 9, { 1, 1, 1, 1, 1 }, 5 }, { "zlib", 0, { 1, 2, 3 }, 3 } };
    for (size_t i = 0; i < 4; i++) {
        mt_atom *packed = value_of(m, E("compress-bytes", S(rounds[i].format), rounds[i].level, mt_array(rounds[i].n, rounds[i].data)));
        assert(answers_are(mt_eval(m, E("decompress-bytes", S(rounds[i].format), mt_keep(packed))), E(inflated(rounds[i].format, packed))) && rounds[i].format);
        mt_drop(packed);
    }

    /* Refusals come before any output. */
    static const int64_t past_a_byte[] = { 256 }, below_a_byte[] = { -1 };
    unsigned char scratch_bytes[4];
    bytes none = { 0 };
    assert(answers_are(guarded(m, E("compress-bytes", "invented", 6, mt_keep(data_list))), E(verdict(computed(compressed("invented", 6, data, 4, &none), &none))))
           && "an invented envelope");
    assert(answers_are(guarded(m, E("compress-bytes", "gzip", -1, mt_keep(data_list))), E(verdict(computed(compressed("gzip", -1, data, 4, &none), &none)))) && "a level below 0");
    assert(answers_are(guarded(m, E("compress-bytes", "gzip", 10, mt_keep(data_list))), E(verdict(computed(compressed("gzip", 10, data, 4, &none), &none)))) && "a level past 9");
    assert(answers_are(guarded(m, E("compress-bytes", "gzip", 6, E(256))), E(verdict(byte_values(past_a_byte, 1, scratch_bytes)))) && "a value past a byte");
    assert(answers_are(guarded(m, E("compress-bytes", "gzip", 6, E(-1))), E(verdict(byte_values(below_a_byte, 1, scratch_bytes)))) && "a value below one");
    static const unsigned char magic_only[] = { 31, 139 };
    bytes gz = bytes_from(gzipped), zl = bytes_from(zlibbed);
    assert(answers_are(guarded(m, E("decompress-bytes", "gzip", mt_unit())), E(verdict(computed(decompressed("gzip", NULL, 0, &none), &none)))) && "no input");
    assert(answers_are(guarded(m, E("decompress-bytes", "gzip", E(31, 139))), E(verdict(computed(decompressed("gzip", magic_only, 2, &none), &none)))) && "a truncated member");
    assert(answers_are(guarded(m, E("decompress-bytes", "zlib", mt_keep(gzipped))), E(verdict(computed(decompressed("zlib", gz.at, gz.n, &none), &none)))) && "gzip read as zlib");
    assert(answers_are(guarded(m, E("decompress-bytes", "gzip", mt_keep(zlibbed))), E(verdict(computed(decompressed("gzip", zl.at, zl.n, &none), &none)))) && "zlib read as gzip");
    free(gz.at);
    free(zl.at);

    /* Files: the engine's in its directory, C's in one of its own beside it. */
    mt_atom *work = value_of(m, E("temp-dir!", T("compression-lib")));
    char c_work[PATH_MAX], parent_of_work[PATH_MAX];
    snprintf(parent_of_work, sizeof parent_of_work, "%s", mt_name(work));
    snprintf(c_work, sizeof c_work, "%s/compression-lib-c-XXXXXX", dirname(parent_of_work));
    require("C mints its own directory", mkdtemp(c_work) != NULL);
    char source[PATH_MAX], packed[PATH_MAX], restored[PATH_MAX], c_source[PATH_MAX], c_packed[PATH_MAX], c_restored[PATH_MAX];
    snprintf(source, sizeof source, "%s", in_dir(mt_name(work), "source"));
    snprintf(packed, sizeof packed, "%s", in_dir(mt_name(work), "packed.gz"));
    snprintf(restored, sizeof restored, "%s", in_dir(mt_name(work), "restored"));
    snprintf(c_source, sizeof c_source, "%s", in_dir(c_work, "source"));
    snprintf(c_packed, sizeof c_packed, "%s", in_dir(c_work, "packed.gz"));
    snprintf(c_restored, sizeof c_restored, "%s", in_dir(c_work, "restored"));
    bytes data_bytes = { 0 };
    put(&data_bytes, data, 4);
    assert(answers_are(mt_eval(m, E("write-bytes!", T(source), mt_keep(data_list))), E(B(published(c_source, &data_bytes)))) && "written");
    assert(answers_are(mt_eval(m, E("compress-file!", "gzip", 6, T(source), T(packed))), E(B(compressed_file("gzip", 6, c_source, c_packed)))) && "a file gzipped");
    bytes engine_packed;
    require("C reads the engine's file", read_file(packed, &engine_packed));
    mt_atom *engine_packed_list = taken(&engine_packed);
    assert(answers_are(mt_eval(m, E("decompress-bytes", "gzip", E("read-bytes!", T(packed)))), E(inflated("gzip", engine_packed_list))) && "which C inflates");
    mt_drop(engine_packed_list);
    assert(answers_are(mt_eval(m, E("decompress-file!", "gzip", T(packed), T(restored))), E(B(decompressed_file("gzip", c_packed, c_restored)))) && "a file restored");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(restored))), E(file_bytes(restored))) && "with its bytes");
    assert(answers_are(mt_eval(m, E("compress-file!", "zlib", 0, T(source), T(source))), E(B(compressed_file("zlib", 0, c_source, c_source)))) && "compressed in place");
    assert(answers_are(mt_eval(m, E("decompress-file!", "zlib", T(source), T(source))), E(B(decompressed_file("zlib", c_source, c_source)))) && "and back in place");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(source))), E(file_bytes(source))) && "the same bytes");
    bytes truncated = { 0 };
    put(&truncated, magic_only, 2);
    assert(answers_are(mt_eval(m, E("write-bytes!", T(packed), E(31, 139))), E(B(published(c_packed, &truncated)))) && "a truncated file written");
    free(truncated.at);
    assert(answers_are(guarded(m, E("decompress-file!", "gzip", T(packed), T(restored))), E(verdict(decompressed_file("gzip", c_packed, c_restored))))
           && "refuses to decode");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(restored))), E(file_bytes(c_restored))) && "and leaves the old file");

    /* Archives: ordinals, metadata, and regular entries only. */
    const char *fixtures = "examples/ch08-data/08-03-the-shipped-libraries/_fixtures";
    char fixture[PATH_MAX], scratch[PATH_MAX];
    snprintf(fixture, sizeof fixture, "%s", in_dir(fixtures, "compression-data.zip"));
    snprintf(scratch, sizeof scratch, "%s", in_dir(c_work, "decoded"));
    mt_atom *entries = value_of(m, E("archive-entries!", T(fixture)));
    entry *listed;
    size_t n_listed = entries_of(fixture, scratch, &listed);
    assert(answers_are(mt_eval(m, E("size-atom", mt_keep(entries))), E((int64_t)n_listed)) && "four entries");
    assert(atom_is(E(mt_keep(mt_at(mt_at(entries, 0), 1)), mt_keep(mt_at(mt_at(entries, 0), 2))), E(0, T(listed[0].name))) && "the first");
    bytes read;
    bool regular = entry_bytes(fixture, scratch, 2, &read);
    require("C reads entry 2", regular);
    assert(answers_are(mt_eval(m, E("archive-read!", T(fixture), 2)), E(taken(&read))) && "entry 2");
    regular = entry_bytes(fixture, scratch, 3, &read);
    require("C reads entry 3", regular);
    assert(answers_are(mt_eval(m, E("archive-read!", T(fixture), 3)), E(taken(&read))) && "entry 3 is empty");
    assert(answers_are(guarded(m, E("archive-read!", T(fixture), 0)), E(verdict(computed(entry_bytes(fixture, scratch, 0, &read), &read)))) && "a directory is no regular entry");
    assert(answers_are(guarded(m, E("archive-read!", T(fixture), 4)), E(verdict(computed(entry_bytes(fixture, scratch, 4, &read), &read)))) && "no entry 4");
    char extracted_dir[PATH_MAX], c_extracted[PATH_MAX];
    snprintf(extracted_dir, sizeof extracted_dir, "%s", in_dir(mt_name(work), "extracted"));
    snprintf(c_extracted, sizeof c_extracted, "%s", in_dir(c_work, "extracted"));
    assert(answers_are(mt_eval(m, E("archive-extract!", T(fixture), T(extracted_dir))), E(B(extracted(fixture, scratch, c_extracted)))) && "extracted");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(in_dir(extracted_dir, "sub/bytes.bin")))), E(file_bytes(in_dir(c_extracted, "sub/bytes.bin")))) && "a nested file");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(in_dir(extracted_dir, "empty")))), E(file_bytes(in_dir(c_extracted, "empty")))) && "an empty one");
    assert(answers_are(guarded(m, E("archive-extract!", T(fixture), T(extracted_dir))), E(verdict(extracted(fixture, scratch, c_extracted))))
           && "not into a full directory");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(in_dir(extracted_dir, "sub/bytes.bin")))), E(file_bytes(in_dir(extracted_dir, "sub/bytes.bin"))))
           && "which keeps its files");

    /* A gzip envelope around the ZIP keeps its entries. */
    assert(answers_are(mt_eval(m, E("compress-file!", "gzip", 6, T(fixture), T(packed))), E(B(compressed_file("gzip", 6, fixture, c_packed)))) && "the archive gzipped");
    entry *wrapped;
    size_t n_wrapped = entries_of(packed, scratch, &wrapped);
    require("C finds the same entries through the engine's gzip", same_entries(listed, n_listed, wrapped, n_wrapped));
    forget_entries(wrapped, n_wrapped);
    assert(answers_are(mt_eval(m, E("archive-entries!", T(packed))), E(mt_keep(entries))) && "the same entries");
    regular = entry_bytes(packed, scratch, 2, &read);
    require("C reads entry 2 through gzip", regular);
    assert(answers_are(mt_eval(m, E("archive-read!", T(packed), 2)), E(taken(&read))) && "entry 2 through gzip");
    char second[PATH_MAX], c_second[PATH_MAX];
    snprintf(second, sizeof second, "%s", in_dir(mt_name(work), "second"));
    snprintf(c_second, sizeof c_second, "%s", in_dir(c_work, "second"));
    assert(answers_are(mt_eval(m, E("archive-extract!", T(packed), T(second))), E(B(extracted(c_packed, scratch, c_second)))) && "extracted through gzip");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(in_dir(second, "sub/bytes.bin")))), E(file_bytes(in_dir(c_second, "sub/bytes.bin")))) && "its nested file");
    forget_entries(listed, n_listed);

    /* Names as APPNOTE decides them. */
    static const char *const named[] = { "compression-unicode.zip", "compression-legacy.zip", "compression-unicode-extra.zip" };
    for (size_t i = 0; i < 3; i++) {
        const char *zip = in_dir(fixtures, named[i]);
        mt_atom *listing = value_of(m, E("archive-entries!", T(zip)));
        char *name = first_name(zip);
        assert(atom_is(mt_keep(mt_at(mt_at(listing, 0), 2)), T(name)) && named[i]);
        free(name);
        mt_drop(listing);
    }

    /* Inspection reads what extraction refuses. */
    char unsafe[PATH_MAX], refused[PATH_MAX], c_refused[PATH_MAX];
    snprintf(unsafe, sizeof unsafe, "%s", in_dir(fixtures, "compression-unsafe.zip"));
    snprintf(refused, sizeof refused, "%s", in_dir(mt_name(work), "refused"));
    snprintf(c_refused, sizeof c_refused, "%s", in_dir(c_work, "refused"));
    regular = entry_bytes(unsafe, scratch, 0, &read);
    require("C reads the unsafe entry", regular);
    assert(answers_are(mt_eval(m, E("archive-read!", T(unsafe), 0)), E(taken(&read))) && "an unsafe name reads");
    assert(answers_are(guarded(m, E("archive-extract!", T(unsafe), T(refused))), E(verdict(extracted(unsafe, scratch, c_refused)))) && "but does not extract");
    assert(answers_are(mt_eval(m, E("dir-exists", T(refused))), E(B(exists_dir(c_refused)))) && "no destination appeared");
    assert(answers_are(mt_eval(m, E("file-exists", T(in_dir(mt_name(work), "escape")))), E(B(exists_file(in_dir(c_work, "escape"))))) && "nothing escaped");
    assert(answers_are(mt_eval(m, E("delete-tree!", mt_keep(work))), E(B(removed_tree(c_work)))) && "the directory removed");

    free(data_bytes.at);
    mt_atom *held[] = { data_list, gzipped, zlibbed, work, entries };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    uselocale(LC_GLOBAL_LOCALE);
    freelocale(utf8);
    return 0;
}
#else
#include <stdio.h>

/* Without zlib and libarchive's headers the program only says what it needs. */
int main(void)
{
    fputs("41-compression_lib.c needs zlib and libarchive: install its development files, then build with\n"
          "cc 41-compression_lib.c $(pkg-config --cflags --libs cmetta zlib libarchive)\n", stderr);
    return 77;
}
#endif
