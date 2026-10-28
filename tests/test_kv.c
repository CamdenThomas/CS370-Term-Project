/* The control socket's reply lines (D-043, D-045) without a socket: the tag-and-fields
 * grammar, every refusal, and a reply cut at every byte. */
#include "interface/kv.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static kv_kind_t parse(const char *s, kv_line_t *l)
{
    return kv_parse_line(s, strlen(s), l);
}

static void test_records(void)
{
    kv_line_t l;

    CHECK(parse("proc name=obdd up=1 pid=412 src=synth", &l) == KV_RECORD, "a record");
    CHECK(strcmp(l.tag, "proc") == 0 && l.nfields == 3 + 1, "tag and four fields");
    CHECK(strcmp(l.f[0].key, "name") == 0 && strcmp(l.f[0].val, "obdd") == 0,
          "fields keep their order");
    CHECK(strcmp(kv_get(&l, "pid"), "412") == 0, "kv_get finds a value");
    CHECK(strcmp(kv_get(&l, "src"), "synth") == 0, "kv_get finds the last field");
    CHECK(kv_get(&l, "missing") == NULL, "kv_get of an absent key");
    CHECK(kv_get(&l, "pi") == NULL && kv_get(&l, "pid2") == NULL,
          "kv_get matches the whole key");

    CHECK(parse("idle", &l) == KV_RECORD && l.nfields == 0 && strcmp(l.tag, "idle") == 0,
          "a tag with no fields");
    CHECK(parse("v a=", &l) == KV_RECORD && strcmp(kv_get(&l, "a"), "") == 0,
          "an empty value");
    CHECK(parse("v t=2026-10-28T10:30:00Z s=coolant,intake x=-1.5e+3 p=/a/b", &l) == KV_RECORD &&
          strcmp(kv_get(&l, "t"), "2026-10-28T10:30:00Z") == 0 &&
          strcmp(kv_get(&l, "s"), "coolant,intake") == 0 &&
          strcmp(kv_get(&l, "x"), "-1.5e+3") == 0, "punctuation inside a value");
    CHECK(parse("k_1 a_b2=c", &l) == KV_RECORD, "digits and underscores in a tag and a key");
}

static void test_terminators(void)
{
    kv_line_t l;

    CHECK(parse("ok", &l) == KV_OK, "ok");
    CHECK(parse("err unavailable storaged is down", &l) == KV_ERR, "err with a reason");
    CHECK(strcmp(l.err_code, "unavailable") == 0, "err code");
    CHECK(strcmp(l.err_reason, "storaged is down") == 0, "the reason keeps its spaces");
    CHECK(parse("err usage", &l) == KV_ERR && strcmp(l.err_code, "usage") == 0 &&
          l.err_reason[0] == '\0', "err with no reason");

    CHECK(parse("err", &l) == KV_BAD, "err with no code");
    CHECK(parse("err ", &l) == KV_BAD, "err with an empty code");
    CHECK(parse("err  x", &l) == KV_BAD, "err with a space where the code goes");
    CHECK(parse("err Bad reason", &l) == KV_BAD, "err code outside [a-z0-9_]");
    CHECK(parse("ok ", &l) == KV_BAD, "ok with a trailing space");
    CHECK(parse("ok a=1", &l) == KV_BAD, "ok is not a tag");
    CHECK(parse("err=1 a=1", &l) == KV_BAD, "err is not a tag either");
    CHECK(parse("OK", &l) == KV_BAD, "OK is not ok");
}

static void test_bad_lines(void)
{
    kv_line_t l;

    CHECK(parse("", &l) == KV_BAD, "empty line");
    CHECK(parse(" proc a=1", &l) == KV_BAD, "leading space");
    CHECK(parse("proc a=1 ", &l) == KV_BAD, "trailing space");
    CHECK(parse("proc  a=1", &l) == KV_BAD, "doubled space");
    CHECK(parse("proc a", &l) == KV_BAD, "a field with no '='");
    CHECK(parse("proc =1", &l) == KV_BAD, "a field with no key");
    CHECK(parse("proc a=b=c", &l) == KV_BAD, "a second '=' in a field");
    CHECK(parse("proc a=1 a=2", &l) == KV_BAD, "a duplicate key");
    CHECK(parse("a=1", &l) == KV_BAD, "a line that starts with a field has no tag");
    CHECK(parse("Proc a=1", &l) == KV_BAD, "uppercase tag");
    CHECK(parse("proc A=1", &l) == KV_BAD, "uppercase key");
    CHECK(parse("pr-oc a=1", &l) == KV_BAD, "punctuation in a tag");
    CHECK(parse("proc a-b=1", &l) == KV_BAD, "punctuation in a key");

    CHECK(kv_parse_line("proc a=\x01", 8, &l) == KV_BAD, "control byte");
    CHECK(kv_parse_line("proc a=1\0", 9, &l) == KV_BAD, "NUL");
    CHECK(parse("proc a=\x7f", &l) == KV_BAD, "DEL");
    CHECK(parse("proc a=\xc3\xa9", &l) == KV_BAD, "non-ASCII");
    CHECK(parse("proc a=1\r", &l) == KV_BAD, "a CR left inside a line");
    CHECK(parse("proc a=1\tb=2", &l) == KV_BAD, "a tab is not a separator");
}

static void test_limits(void)
{
    kv_line_t l;
    char s[KV_MAX_LINE + 64];

    /* Tag lengths: KV_MAX_TAG counts the NUL. */
    memset(s, 't', KV_MAX_TAG);
    CHECK(kv_parse_line(s, KV_MAX_TAG - 1, &l) == KV_RECORD, "longest tag");
    CHECK(kv_parse_line(s, KV_MAX_TAG, &l) == KV_BAD, "a tag one too long");

    /* Keys and values. */
    char key[KV_MAX_KEY + 2], val[KV_MAX_VALUE + 2];
    memset(key, 'k', sizeof key); key[KV_MAX_KEY - 1] = '\0';
    snprintf(s, sizeof s, "t %s=1", key);
    CHECK(parse(s, &l) == KV_RECORD, "longest key");
    key[KV_MAX_KEY - 1] = 'k'; key[KV_MAX_KEY] = '\0';
    snprintf(s, sizeof s, "t %s=1", key);
    CHECK(parse(s, &l) == KV_BAD, "a key one too long");

    memset(val, 'v', sizeof val); val[KV_MAX_VALUE - 1] = '\0';
    snprintf(s, sizeof s, "t a=%s", val);
    CHECK(parse(s, &l) == KV_RECORD && strlen(kv_get(&l, "a")) == KV_MAX_VALUE - 1,
          "longest value");
    val[KV_MAX_VALUE - 1] = 'v'; val[KV_MAX_VALUE] = '\0';
    snprintf(s, sizeof s, "t a=%s", val);
    CHECK(parse(s, &l) == KV_BAD, "a value one too long");

    /* Fields: sixteen fit, seventeen do not. */
    s[0] = 't'; s[1] = '\0';
    for (unsigned i = 0; i < KV_MAX_FIELDS; i++)
        snprintf(s + strlen(s), sizeof s - strlen(s), " k%u=%u", i, i);
    CHECK(parse(s, &l) == KV_RECORD && l.nfields == KV_MAX_FIELDS, "sixteen fields");
    snprintf(s + strlen(s), sizeof s - strlen(s), " kx=1");
    CHECK(parse(s, &l) == KV_BAD, "seventeen fields");

    /* The line cap: 1 + 5 * 99 + 16 = 512 bytes is the longest line, 513 is refused. */
    char big[KV_MAX_LINE + 64];
    char longv[KV_MAX_VALUE];
    memset(longv, 'v', KV_MAX_VALUE - 1); longv[KV_MAX_VALUE - 1] = '\0';
    for (size_t last = 12; last <= 13; last++) {
        char tail[32];
        memset(tail, 'w', last); tail[last] = '\0';
        int n = snprintf(big, sizeof big, "t");
        for (int i = 0; i < 5; i++) n += snprintf(big + n, sizeof big - (size_t)n, " k%d=%s", i, longv);
        n += snprintf(big + n, sizeof big - (size_t)n, " k5=%s", tail);
        CHECK((size_t)n == KV_MAX_LINE - 12 + last, "built the line I meant to build");
        CHECK(kv_parse_line(big, (size_t)n, &l) == (last == 12 ? KV_RECORD : KV_BAD),
              last == 12 ? "a 512-byte line" : "a 513-byte line");
    }
}

static void test_cleared_between_lines(void)
{
    kv_line_t l;

    CHECK(parse("proc a=1 b=2", &l) == KV_RECORD, "first");
    CHECK(parse("idle", &l) == KV_RECORD && l.nfields == 0 && kv_get(&l, "a") == NULL,
          "a later line does not inherit the earlier one's fields");
    CHECK(parse("err usage bad verb", &l) == KV_ERR, "an err line");
    CHECK(parse("err usage", &l) == KV_ERR && l.err_reason[0] == '\0',
          "nor its reason");
}

static void test_reply_next(void)
{
    const char reply[] = "proc name=obdd\r\nproc name=storaged\nbad line=\x01\nok\r\n";
    const char *cur = reply, *end = reply + sizeof reply - 1;
    kv_line_t l;

    CHECK(kv_reply_next(&cur, end, &l) == KV_RECORD && strcmp(kv_get(&l, "name"), "obdd") == 0,
          "first line, CRLF");
    CHECK(kv_reply_next(&cur, end, &l) == KV_RECORD &&
          strcmp(kv_get(&l, "name"), "storaged") == 0, "second line, LF");
    const char *before = cur;
    CHECK(kv_reply_next(&cur, end, &l) == KV_BAD && cur > before,
          "a bad line is reported and skipped");
    CHECK(kv_reply_next(&cur, end, &l) == KV_OK && cur == end, "the terminator");
    CHECK(kv_reply_next(&cur, end, &l) == KV_INCOMPLETE && cur == end, "nothing left");

    const char part[] = "proc a=1";
    cur = part; end = part + sizeof part - 1;
    CHECK(kv_reply_next(&cur, end, &l) == KV_INCOMPLETE && cur == part,
          "a line with no newline waits, and the cursor stays");
}

static void test_reply_check(void)
{
    kv_line_t f;
    const char *good = "proc name=obdd up=1\r\nproc name=storaged up=0\r\nok\r\n";
    const char *errs = "err unavailable storaged is down\r\n";

    CHECK(kv_reply_check(good, strlen(good), NULL) == KV_OK, "a good reply");
    CHECK(kv_reply_check("ok\n", 3, NULL) == KV_OK, "a reply with no records");

    memset(&f, 0, sizeof f);
    CHECK(kv_reply_check(errs, strlen(errs), &f) == KV_ERR, "a refused reply");
    CHECK(strcmp(f.err_code, "unavailable") == 0 && strcmp(f.err_reason, "storaged is down") == 0,
          "the final line is handed back");
    CHECK(kv_reply_check("proc a=1\nerr x y\n", 17, &f) == KV_ERR &&
          strcmp(f.err_reason, "y") == 0, "records, then err");

    CHECK(kv_reply_check("", 0, NULL) == KV_INCOMPLETE, "an empty buffer");
    CHECK(kv_reply_check("proc a=1\n", 9, NULL) == KV_INCOMPLETE, "no terminator");
    CHECK(kv_reply_check("proc a=1\nok", 11, NULL) == KV_INCOMPLETE,
          "a terminator whose newline has not arrived");

    CHECK(kv_reply_check("ok\nproc a=1\n", 12, NULL) == KV_BAD, "a record after ok");
    CHECK(kv_reply_check("ok\nok\n", 6, NULL) == KV_BAD, "two terminators");
    CHECK(kv_reply_check("err a b\nok\n", 11, NULL) == KV_BAD, "ok after err");
    CHECK(kv_reply_check("proc a=1\nbad line\nok\n", 21, NULL) == KV_BAD,
          "a bad line in the middle");
}

static void test_every_split_is_incomplete_until_whole(void)
{
    const char *reply = "proc name=obdd up=1\r\nproc name=storaged up=0\r\nok\r\n";
    size_t n = strlen(reply);

    for (size_t cut = 0; cut < n; cut++)
        CHECK(kv_reply_check(reply, cut, NULL) == KV_INCOMPLETE, "a prefix is incomplete");
    CHECK(kv_reply_check(reply, n, NULL) == KV_OK, "the whole reply");
}

int main(void)
{
    log_init("test_kv", LOG_FATAL);
    test_records();
    test_terminators();
    test_bad_lines();
    test_limits();
    test_cleared_between_lines();
    test_reply_next();
    test_reply_check();
    test_every_split_is_incomplete_until_whole();
    return failures == 0 ? 0 : 1;
}
