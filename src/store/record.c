#include "store/record.h"

#include "common/log.h"

#include <string.h>

/* The body size each kind allows; an event's is 4 + its text length, checked apart. */
static int body_len_ok(uint16_t kind, uint16_t body_len)
{
    switch ((rec_kind_t)kind) {
    case REC_SAMPLE:   return body_len == sizeof(rec_sample_t);
    case REC_BEGIN:    return body_len == sizeof(rec_begin_t);
    case REC_COMMIT:   return body_len == sizeof(rec_commit_t);
    case REC_BOOT:     return body_len == sizeof(rec_boot_t);
    case REC_SHUTDOWN: return body_len == sizeof(rec_shutdown_t);
    case REC_EVENT:    return body_len >= 4u && body_len <= REC_MAX_BODY;
    }
    return 0;
}

static int kind_ok(uint16_t kind)
{
    return kind >= REC_SAMPLE && kind <= REC_EVENT;
}

/* An event's len field must account for exactly the rest of its body. */
static int event_ok(const uint8_t *body, uint16_t body_len)
{
    uint16_t text_len;
    memcpy(&text_len, body + 2, sizeof text_len);
    return text_len <= REC_EVENT_TEXT_MAX && (uint32_t)text_len + 4u == body_len;
}

size_t rec_encode(uint8_t *out, size_t cap, const rec_hdr_t *hdr, const void *body)
{
    if (out == NULL || hdr == NULL || body == NULL) return 0;
    if (!kind_ok(hdr->kind) || !body_len_ok(hdr->kind, hdr->body_len)) return 0;
    if (!SRC_IS_VALID(hdr->src)) return 0;
    if (hdr->kind == REC_EVENT && !event_ok(body, hdr->body_len)) return 0;

    size_t total = REC_HDR_SIZE + (size_t)hdr->body_len + REC_CRC_SIZE;
    if (cap < total) return 0;

    rec_hdr_t h = *hdr;
    h.magic    = REC_MAGIC;
    h.reserved = 0;
    memcpy(out, &h, REC_HDR_SIZE);
    memcpy(out + REC_HDR_SIZE, body, hdr->body_len);

    uint32_t crc = rec_crc32(out, (uint32_t)(REC_HDR_SIZE + hdr->body_len));
    memcpy(out + REC_HDR_SIZE + hdr->body_len, &crc, REC_CRC_SIZE);
    return total;
}

rec_status_t rec_decode(const uint8_t *buf, size_t len, rec_hdr_t *hdr,
                        const uint8_t **body, size_t *size)
{
    if (len < REC_HDR_SIZE) return REC_SHORT;

    rec_hdr_t h;
    memcpy(&h, buf, REC_HDR_SIZE);
    if (h.magic != REC_MAGIC) return REC_BAD_MAGIC;
    if (!kind_ok(h.kind)) return REC_BAD_KIND;
    if (!body_len_ok(h.kind, h.body_len)) return REC_BAD_LEN;

    size_t total = REC_HDR_SIZE + (size_t)h.body_len + REC_CRC_SIZE;
    if (len < total) return REC_SHORT;

    uint32_t stored;
    memcpy(&stored, buf + REC_HDR_SIZE + h.body_len, REC_CRC_SIZE);
    if (rec_crc32(buf, (uint32_t)(REC_HDR_SIZE + h.body_len)) != stored) return REC_BAD_CRC;

    if (!SRC_IS_VALID(h.src)) return REC_BAD_SRC;
    if (h.kind == REC_EVENT && !event_ok(buf + REC_HDR_SIZE, h.body_len)) return REC_BAD_LEN;

    if (hdr != NULL)  *hdr = h;
    if (body != NULL) *body = buf + REC_HDR_SIZE;
    if (size != NULL) *size = total;
    return REC_OK;
}

const char *rec_status_name(rec_status_t st)
{
    switch (st) {
    case REC_OK:        return "ok";
    case REC_SHORT:     return "short";
    case REC_BAD_MAGIC: return "bad_magic";
    case REC_BAD_KIND:  return "bad_kind";
    case REC_BAD_LEN:   return "bad_len";
    case REC_BAD_CRC:   return "bad_crc";
    case REC_BAD_SRC:   return "bad_src";
    }
    return "unknown";
}
