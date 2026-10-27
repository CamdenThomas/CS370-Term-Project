/* The record format is the file format (D-034): every kind must round-trip, and no
 * damaged or partial record may ever decode as valid, because recovery trusts exactly
 * that. */
#include "store/record.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static rec_hdr_t hdr(uint16_t kind, uint16_t body_len)
{
    rec_hdr_t h;
    memset(&h, 0, sizeof h);
    h.kind     = kind;
    h.body_len = body_len;
    h.seq      = 42;
    h.mono_ns  = 123456789u;
    h.wall_ns  = 1793000000000000000u;
    h.src      = SRC_SYNTH;   /* test data is synthesized; label it (CLAUDE.md §2.7) */
    h.flags    = REC_FLAG_WALL_UNTRUSTED;
    h.boot_id  = 7;
    return h;
}

static rec_sample_t sample(void)
{
    rec_sample_t s;
    memset(&s, 0, sizeof s);
    s.mode = 0x01; s.pid = 0x0C; s.raw_len = 2; s.raw[0] = 0x1A; s.raw[1] = 0xF8;
    s.value = 1726.0f;
    return s;
}

static void test_sample_round_trips(void)
{
    uint8_t buf[REC_MAX_SIZE];
    rec_hdr_t in = hdr(REC_SAMPLE, sizeof(rec_sample_t));
    rec_sample_t s = sample();
    size_t n = rec_encode(buf, sizeof buf, &in, &s);
    CHECK(n == REC_HDR_SIZE + sizeof s + REC_CRC_SIZE, "a sample record is 56 bytes");

    rec_hdr_t out; const uint8_t *body; size_t size;
    CHECK(rec_decode(buf, n, &out, &body, &size) == REC_OK, "sample decodes");
    CHECK(size == n, "decoded size");
    CHECK(out.magic == REC_MAGIC && out.seq == 42 && out.boot_id == 7 &&
          out.src == SRC_SYNTH && out.flags == REC_FLAG_WALL_UNTRUSTED &&
          out.mono_ns == in.mono_ns && out.wall_ns == in.wall_ns, "header fields");
    rec_sample_t got;
    memcpy(&got, body, sizeof got);
    CHECK(memcmp(&got, &s, sizeof s) == 0, "sample body");
    CHECK(memcmp(buf, "CWR1", 4) == 0, "magic reads CWR1 on disk");
}

static void test_every_kind_round_trips(void)
{
    uint8_t buf[REC_MAX_SIZE];
    rec_begin_t b = { .txn_id = 9 };
    rec_commit_t c = { .txn_id = 9, .records = 64, .reserved = 0 };
    rec_boot_t boot = { .prev_end = REC_PREV_RECOVERED, .bytes_truncated = 17,
                        .records_lost = 1 };
    rec_shutdown_t sd = { .reason = REC_SHUT_SOURCE_LOST };
    rec_event_t ev = { .code = 0x0201, .len = 0 };
    const char *text = "link_lost port=/dev/obd";
    ev.len = (uint16_t)strlen(text);
    memcpy(ev.text, text, ev.len);

    struct { uint16_t kind; uint16_t len; const void *body; } cases[] = {
        { REC_BEGIN,    sizeof b,    &b },
        { REC_COMMIT,   sizeof c,    &c },
        { REC_BOOT,     sizeof boot, &boot },
        { REC_SHUTDOWN, sizeof sd,   &sd },
        { REC_EVENT,    (uint16_t)(4u + ev.len), &ev },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        rec_hdr_t in = hdr(cases[i].kind, cases[i].len);
        size_t n = rec_encode(buf, sizeof buf, &in, cases[i].body);
        CHECK(n == REC_HDR_SIZE + cases[i].len + REC_CRC_SIZE, "encoded size");
        rec_hdr_t out; const uint8_t *body; size_t size;
        CHECK(rec_decode(buf, n, &out, &body, &size) == REC_OK, "kind decodes");
        CHECK(out.kind == cases[i].kind && size == n, "kind and size");
        CHECK(memcmp(body, cases[i].body, cases[i].len) == 0, "body bytes");
    }
}

/* A one-byte change anywhere in a record must be caught. */
static void test_every_flipped_byte_is_rejected(void)
{
    uint8_t buf[REC_MAX_SIZE];
    rec_hdr_t in = hdr(REC_SAMPLE, sizeof(rec_sample_t));
    rec_sample_t s = sample();
    size_t n = rec_encode(buf, sizeof buf, &in, &s);
    for (size_t i = 0; i < n; i++) {
        for (unsigned bit = 0; bit < 8; bit++) {
            buf[i] ^= (uint8_t)(1u << bit);
            CHECK(rec_decode(buf, n, NULL, NULL, NULL) != REC_OK, "flipped bit accepted");
            buf[i] ^= (uint8_t)(1u << bit);
        }
    }
    CHECK(rec_decode(buf, n, NULL, NULL, NULL) == REC_OK, "restored record decodes");
}

/* A record cut at any length is short, never valid: the torn tail recovery truncates. */
static void test_every_truncation_is_short(void)
{
    uint8_t buf[REC_MAX_SIZE];
    rec_hdr_t in = hdr(REC_SAMPLE, sizeof(rec_sample_t));
    rec_sample_t s = sample();
    size_t n = rec_encode(buf, sizeof buf, &in, &s);
    for (size_t cut = 0; cut < n; cut++)
        CHECK(rec_decode(buf, cut, NULL, NULL, NULL) == REC_SHORT, "cut record not short");
}

static void test_encoder_refuses_what_decoder_would_reject(void)
{
    uint8_t buf[REC_MAX_SIZE];
    rec_sample_t s = sample();

    rec_hdr_t h = hdr(REC_SAMPLE, sizeof s);
    h.src = SRC_INVALID;
    CHECK(rec_encode(buf, sizeof buf, &h, &s) == 0, "unlabeled record encoded (D-040)");

    h = hdr(0, sizeof s);
    CHECK(rec_encode(buf, sizeof buf, &h, &s) == 0, "kind 0 encoded");
    h = hdr(REC_EVENT + 1, sizeof s);
    CHECK(rec_encode(buf, sizeof buf, &h, &s) == 0, "unknown kind encoded");

    h = hdr(REC_SAMPLE, sizeof s + 1);
    CHECK(rec_encode(buf, sizeof buf, &h, &s) == 0, "wrong body_len encoded");

    h = hdr(REC_SAMPLE, sizeof s);
    CHECK(rec_encode(buf, REC_HDR_SIZE + sizeof s + REC_CRC_SIZE - 1, &h, &s) == 0,
          "record encoded past cap");

    rec_event_t ev = { .code = 1, .len = REC_EVENT_TEXT_MAX + 1 };
    h = hdr(REC_EVENT, REC_MAX_BODY);
    CHECK(rec_encode(buf, sizeof buf, &h, &ev) == 0, "oversized event encoded");
    ev.len = 3;
    h = hdr(REC_EVENT, 4 + 4);
    CHECK(rec_encode(buf, sizeof buf, &h, &ev) == 0, "event len disagrees with body_len");
}

static void test_decoder_names_what_is_wrong(void)
{
    uint8_t buf[REC_MAX_SIZE];
    rec_hdr_t in = hdr(REC_SAMPLE, sizeof(rec_sample_t));
    rec_sample_t s = sample();
    size_t n = rec_encode(buf, sizeof buf, &in, &s);

    buf[0] ^= 0xFF;
    CHECK(rec_decode(buf, n, NULL, NULL, NULL) == REC_BAD_MAGIC, "bad magic");
    buf[0] ^= 0xFF;

    /* A CRC-valid record whose src is 0 is a writer bug, not damage: name it. */
    rec_hdr_t bad;
    memcpy(&bad, buf, sizeof bad);
    bad.src = SRC_INVALID;
    memcpy(buf, &bad, sizeof bad);
    uint32_t crc = rec_crc32(buf, (uint32_t)(n - REC_CRC_SIZE));
    memcpy(buf + n - REC_CRC_SIZE, &crc, sizeof crc);
    CHECK(rec_decode(buf, n, NULL, NULL, NULL) == REC_BAD_SRC, "unlabeled record decoded");

    CHECK(strcmp(rec_status_name(REC_BAD_CRC), "bad_crc") == 0, "status name");
}

int main(void)
{
    test_sample_round_trips();
    test_every_kind_round_trips();
    test_every_flipped_byte_is_rejected();
    test_every_truncation_is_short();
    test_encoder_refuses_what_decoder_would_reject();
    test_decoder_names_what_is_wrong();
    return failures == 0 ? 0 : 1;
}
