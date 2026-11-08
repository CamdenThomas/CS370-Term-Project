/* ELM327 reply text and Mode 01 / 09 decoding: the C port of tools/obdlib.py. Each
 * function names the Python it follows; tests/test_pid.c ports tests/tools/test_obdlib.py
 * case for case. */
#include "obd/pid.h"

#include "common/sha256.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* A reply line longer than this is not one the ELM327 sends; it is kept as "seen" but
 * matches neither a known text nor a hex reply. */
#define LINE_CHARS 256u
#define LINE_BYTES (LINE_CHARS / 2u)

/* obdlib._ASLEEP_TEXT, _ERROR_TEXT, _SKIP_TEXT, in the same order. */
static const char *const k_asleep[] = { "UNABLE TO CONNECT", "CAN ERROR" };
static const char *const k_error[] = {
    "?", "BUFFER FULL", "STOPPED",
    "BUS BUSY", "BUS ERROR", "DATA ERROR", "<DATA ERROR", "FB ERROR",
    "LV RESET", "ACT ALERT", "BUS INIT: ...ERROR", "BUS INIT: ERROR",
    "BUS INIT:...ERROR", "ERROR",
};
static const char *const k_skip[] = {
    "SEARCHING...", "BUS INIT: ...OK", "BUS INIT: OK", "BUS INIT:...OK",
};

#define COUNT(a) (sizeof (a) / sizeof (a)[0])

static bool in_list(const char *line, const char *const *list, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(line, list[i]) == 0) return true;
    return false;
}

/* Copies text, cut to fit: every text it is given from the lists above fits; a reply
 * line it is given only ever matched one of them. */
static void set_text(char out[OBD_TEXT_MAX], const char *text)
{
    size_t n = strlen(text);
    if (n > OBD_TEXT_MAX - 1) n = OBD_TEXT_MAX - 1;
    memcpy(out, text, n);
    out[n] = '\0';
}

/* --- lines: obdlib._lines ------------------------------------------------------------- */

typedef struct {
    const char *p, *end;
} lines_t;

typedef struct {
    char text[LINE_CHARS + 1];
    bool too_long;
} line_t;

/* The next non-empty line: '>' and NUL removed, split on runs of CR and LF, stripped,
 * upper-cased. Returns false when no line is left. */
static bool next_line(lines_t *it, line_t *out)
{
    while (it->p < it->end) {
        size_t n = 0;
        out->too_long = false;
        while (it->p < it->end && *it->p != '\r' && *it->p != '\n') {
            char c = *it->p++;
            if (c == '>' || c == '\0') continue;
            if (n < LINE_CHARS) out->text[n++] = (char)toupper((unsigned char)c);
            else out->too_long = true;
        }
        while (it->p < it->end && (*it->p == '\r' || *it->p == '\n')) it->p++;

        size_t start = 0;
        while (start < n && isspace((unsigned char)out->text[start])) start++;
        while (n > start && isspace((unsigned char)out->text[n - 1])) n--;
        if (n == start && !out->too_long) continue;   /* blank: Python drops it */
        memmove(out->text, out->text + start, n - start);
        out->text[n - start] = '\0';
        return true;
    }
    return false;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* obdlib._hex_bytes: a line of hex pairs, spaces allowed anywhere, as bytes. False if
 * the line is empty, holds anything else, or has an odd number of digits. */
static bool hex_bytes(const char *s, uint8_t out[LINE_BYTES], size_t *n)
{
    size_t got = 0;
    int hi = -1;
    for (; *s != '\0'; s++) {
        if (*s == ' ') continue;
        int v = hexval(*s);
        if (v < 0) return false;
        if (hi < 0) {
            hi = v;
        } else {
            if (got == LINE_BYTES) return false;
            out[got++] = (uint8_t)(hi << 4 | v);
            hi = -1;
        }
    }
    if (hi >= 0 || got == 0) return false;
    *n = got;
    return true;
}

/* obdlib._check_text: OBD_OK, or the failure this line names (and its text). */
static obd_status_t check_text(const line_t *l, char text[OBD_TEXT_MAX])
{
    if (l->too_long) return OBD_OK;
    obd_status_t st = OBD_OK;
    if (strcmp(l->text, "NO DATA") == 0) st = OBD_NODATA;
    else if (in_list(l->text, k_asleep, COUNT(k_asleep))) st = OBD_ASLEEP;
    else if (in_list(l->text, k_error, COUNT(k_error))) st = OBD_ERROR;
    if (st != OBD_OK) set_text(text, l->text);
    return st;
}

/* --- requests and replies ------------------------------------------------------------- */

void obd_request(char out[5], uint8_t mode, uint8_t pid)
{
    snprintf(out, 5, "%02X%02X", (unsigned)mode, (unsigned)pid);
}

obd_status_t obd_parse_all(const char *raw, size_t len, uint8_t mode, uint8_t pid,
                           obd_answer_t *out)
{
    memset(out, 0, sizeof *out);
    lines_t it = { raw, raw + len };
    line_t l;
    bool seen = false;
    uint8_t want = (uint8_t)(mode + 0x40u);

    while (next_line(&it, &l)) {
        seen = true;
        if (!l.too_long && in_list(l.text, k_skip, COUNT(k_skip))) continue;
        obd_status_t st = check_text(&l, out->text);
        if (st != OBD_OK) return st;

        uint8_t b[LINE_BYTES];
        size_t n;
        if (l.too_long || !hex_bytes(l.text, b, &n) || n < 2 || b[0] != want || b[1] != pid)
            continue;
        out->seen++;
        if (out->n < OBD_MAX_ECUS) {
            obd_line_t *e = &out->ecu[out->n++];
            size_t keep = n - 2 < OBD_MAX_DATA ? n - 2 : OBD_MAX_DATA;
            e->len = (uint8_t)keep;
            memcpy(e->data, b + 2, keep);
        }
    }
    if (out->seen == 0) {
        set_text(out->text, seen ? "no matching reply" : "empty reply");
        return OBD_ERROR;
    }
    return OBD_OK;
}

obd_status_t obd_check_bus(const char *raw, size_t len)
{
    lines_t it = { raw, raw + len };
    line_t l;
    while (next_line(&it, &l))
        if (!l.too_long && in_list(l.text, k_asleep, COUNT(k_asleep))) return OBD_ASLEEP;
    return OBD_OK;
}

/* --- Mode 01 decoding: obdlib.PIDS ------------------------------------------------------ */

/* Each formula is written in the Python's order of operations, in double, so a value
 * decodes to the same bits in both. */
static double u16(const uint8_t *d)       { return (double)(d[0] * 256 + d[1]); }
static double f_pct(const uint8_t *d)     { return d[0] * 100.0 / 255.0; }
static double f_temp(const uint8_t *d)    { return (double)(d[0] - 40); }
static double f_trim(const uint8_t *d)    { return (d[0] - 128) * 100.0 / 128.0; }
static double f_fuel_press(const uint8_t *d)   { return (double)(d[0] * 3); }
static double f_byte(const uint8_t *d)    { return (double)d[0]; }
static double f_rpm(const uint8_t *d)     { return u16(d) / 4.0; }
static double f_timing(const uint8_t *d)  { return d[0] / 2.0 - 64.0; }
static double f_maf(const uint8_t *d)     { return u16(d) / 100.0; }
static double f_volts(const uint8_t *d)   { return u16(d) / 1000.0; }
static double f_abs_load(const uint8_t *d) { return u16(d) * 100.0 / 255.0; }
static double f_ratio(const uint8_t *d)   { return u16(d) / 32768.0; }
static double f_fuel_rate(const uint8_t *d){ return u16(d) / 20.0; }
static double f_o2v(const uint8_t *d)     { return d[0] / 200.0; }
static double f_cat(const uint8_t *d)     { return u16(d) / 10.0 - 40.0; }

typedef struct {
    uint8_t     pid;
    uint8_t     need;     /* data bytes the formula reads */
    const char *name;
    const char *unit;
    double    (*fn)(const uint8_t *d);
} pid_def_t;

static const pid_def_t k_pids[] = {
    { 0x04, 1, "Calculated engine load", "%", f_pct },
    { 0x05, 1, "Coolant temperature", "degC", f_temp },
    { 0x06, 1, "Short term fuel trim, bank 1", "%", f_trim },
    { 0x07, 1, "Long term fuel trim, bank 1", "%", f_trim },
    { 0x08, 1, "Short term fuel trim, bank 2", "%", f_trim },
    { 0x09, 1, "Long term fuel trim, bank 2", "%", f_trim },
    { 0x0A, 1, "Fuel pressure", "kPa", f_fuel_press },
    { 0x0B, 1, "Intake manifold pressure", "kPa", f_byte },
    { 0x0C, 2, "Engine speed", "rpm", f_rpm },
    { 0x0D, 1, "Vehicle speed", "km/h", f_byte },
    { 0x0E, 1, "Timing advance", "deg", f_timing },
    { 0x0F, 1, "Intake air temperature", "degC", f_temp },
    { 0x10, 2, "Mass air flow", "g/s", f_maf },
    { 0x11, 1, "Throttle position", "%", f_pct },
    /* Narrow-band O2 sensors report volts first (the short term trim is second). */
    { 0x14, 2, "O2 sensor 1 voltage", "V", f_o2v },
    { 0x15, 2, "O2 sensor 2 voltage", "V", f_o2v },
    { 0x16, 2, "O2 sensor 3 voltage", "V", f_o2v },
    { 0x17, 2, "O2 sensor 4 voltage", "V", f_o2v },
    { 0x18, 2, "O2 sensor 5 voltage", "V", f_o2v },
    { 0x19, 2, "O2 sensor 6 voltage", "V", f_o2v },
    { 0x1A, 2, "O2 sensor 7 voltage", "V", f_o2v },
    { 0x1B, 2, "O2 sensor 8 voltage", "V", f_o2v },
    { 0x1F, 2, "Run time since start", "s", u16 },
    { 0x21, 2, "Distance with MIL on", "km", u16 },
    /* Wide-band O2 sensors report the equivalence ratio first. */
    { 0x24, 4, "O2 sensor 1 lambda", "ratio", f_ratio },
    { 0x25, 4, "O2 sensor 2 lambda", "ratio", f_ratio },
    { 0x26, 4, "O2 sensor 3 lambda", "ratio", f_ratio },
    { 0x27, 4, "O2 sensor 4 lambda", "ratio", f_ratio },
    { 0x28, 4, "O2 sensor 5 lambda", "ratio", f_ratio },
    { 0x29, 4, "O2 sensor 6 lambda", "ratio", f_ratio },
    { 0x2A, 4, "O2 sensor 7 lambda", "ratio", f_ratio },
    { 0x2B, 4, "O2 sensor 8 lambda", "ratio", f_ratio },
    { 0x2C, 1, "Commanded EGR", "%", f_pct },
    { 0x2F, 1, "Fuel level", "%", f_pct },
    { 0x30, 1, "Warm-ups since codes cleared", "count", f_byte },
    { 0x31, 2, "Distance since codes cleared", "km", u16 },
    { 0x33, 1, "Barometric pressure", "kPa", f_byte },
    { 0x3C, 2, "Catalyst temperature B1S1", "degC", f_cat },
    { 0x3D, 2, "Catalyst temperature B2S1", "degC", f_cat },
    { 0x3E, 2, "Catalyst temperature B1S2", "degC", f_cat },
    { 0x3F, 2, "Catalyst temperature B2S2", "degC", f_cat },
    { 0x42, 2, "Control module voltage", "V", f_volts },
    { 0x43, 2, "Absolute load", "%", f_abs_load },
    { 0x44, 2, "Commanded equivalence ratio", "ratio", f_ratio },
    { 0x45, 1, "Relative throttle position", "%", f_pct },
    { 0x46, 1, "Ambient air temperature", "degC", f_temp },
    { 0x47, 1, "Absolute throttle position B", "%", f_pct },
    { 0x49, 1, "Accelerator pedal position D", "%", f_pct },
    { 0x4A, 1, "Accelerator pedal position E", "%", f_pct },
    { 0x4C, 1, "Commanded throttle actuator", "%", f_pct },
    { 0x5C, 1, "Engine oil temperature", "degC", f_temp },
    { 0x5E, 2, "Engine fuel rate", "L/h", f_fuel_rate },
};

static const pid_def_t *find(uint8_t pid)
{
    for (size_t i = 0; i < COUNT(k_pids); i++)
        if (k_pids[i].pid == pid) return &k_pids[i];
    return NULL;
}

bool obd_decode(uint8_t pid, const uint8_t *data, size_t len, double *value)
{
    const pid_def_t *d = find(pid);
    if (d == NULL || len < d->need) return false;
    *value = d->fn(data);
    return true;
}

const char *obd_pid_name(uint8_t pid) { const pid_def_t *d = find(pid); return d ? d->name : ""; }
const char *obd_pid_unit(uint8_t pid) { const pid_def_t *d = find(pid); return d ? d->unit : ""; }
bool obd_pid_known(uint8_t pid)       { return find(pid) != NULL; }

/* --- support bitmaps -------------------------------------------------------------------- */

int obd_support_pids(uint8_t base, const uint8_t *data, size_t len, obd_pidset_t *set)
{
    if (len != 4) return -1;
    uint32_t bits = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 |
                    (uint32_t)data[2] << 8 | (uint32_t)data[3];
    for (unsigned i = 0; i < 32; i++) {
        unsigned p = base + 1u + i;
        /* Past 0xFF only the 0xE0 block's next-block bit could land; no PID is there. */
        if (p <= 0xFFu && (bits & (1u << (31u - i)))) obd_pidset_add(set, (uint8_t)p);
    }
    return 0;
}

obd_status_t obd_support_from_reply(const char *raw, size_t len, uint8_t base,
                                    obd_pidset_t *set)
{
    obd_answer_t a;
    obd_status_t st = obd_parse_all(raw, len, 0x01, base, &a);
    if (st != OBD_OK) return st;
    for (size_t i = 0; i < a.n; i++)
        if (obd_support_pids(base, a.ecu[i].data, a.ecu[i].len, set) != 0) return OBD_ERROR;
    return OBD_OK;
}

/* --- VIN: obdlib.decode_vin and vin_hash ---------------------------------------------- */

#define VIN_FRAMES 16u   /* a VIN needs 3 multi-frame lines or 5 single frames */

typedef struct {
    bool    have;
    uint8_t len;
    uint8_t d[LINE_BYTES];
} frame_t;

/* "<digits>:<rest>" -> index and rest, as the Python's ^(\d+):\s*(.*)$. */
static bool framed_line(const char *s, unsigned *index, const char **rest)
{
    if (!isdigit((unsigned char)*s)) return false;
    unsigned v = 0;
    while (isdigit((unsigned char)*s)) {
        if (v < 1000u) v = v * 10u + (unsigned)(*s - '0');
        s++;
    }
    if (*s != ':') return false;
    s++;
    while (isspace((unsigned char)*s)) s++;
    *index = v;
    *rest = s;
    return true;
}

obd_status_t obd_decode_vin(const char *raw, size_t len, char vin[OBD_VIN_MAX + 1])
{
    frame_t framed[VIN_FRAMES], chunks[VIN_FRAMES];
    memset(framed, 0, sizeof framed);
    memset(chunks, 0, sizeof chunks);
    bool any_framed = false, any_chunk = false;
    char text[OBD_TEXT_MAX];
    lines_t it = { raw, raw + len };
    line_t l;
    vin[0] = '\0';

    while (next_line(&it, &l)) {
        obd_status_t st = check_text(&l, text);
        if (st != OBD_OK) return st;
        if (l.too_long) continue;

        uint8_t b[LINE_BYTES];
        size_t n;
        unsigned index;
        const char *rest;
        if (framed_line(l.text, &index, &rest)) {
            /* A repeated index is a second ECU: keep the first. */
            if (hex_bytes(rest, b, &n) && index < VIN_FRAMES && !framed[index].have) {
                framed[index].have = true;
                framed[index].len = (uint8_t)n;
                memcpy(framed[index].d, b, n);
                any_framed = true;
            }
            continue;
        }
        if (hex_bytes(l.text, b, &n) && n >= 3 && b[0] == 0x49 && b[1] == 0x02 &&
            b[2] < VIN_FRAMES && !chunks[b[2]].have) {
            chunks[b[2]].have = true;   /* the first ECU's chunk per sequence number */
            chunks[b[2]].len = (uint8_t)(n - 3);
            memcpy(chunks[b[2]].d, b + 3, n - 3);
            any_chunk = true;
        }
    }
    if (!any_framed && !any_chunk) return OBD_ERROR;   /* "no VIN in reply" */

    uint8_t body[VIN_FRAMES * LINE_BYTES];
    size_t blen = 0;
    const frame_t *src = any_framed ? framed : chunks;
    for (unsigned i = 0; i < VIN_FRAMES; i++) {
        if (!src[i].have) continue;
        memcpy(body + blen, src[i].d, src[i].len);
        blen += src[i].len;
    }
    size_t skip = 0;
    if (any_framed && blen >= 2 && body[0] == 0x49 && body[1] == 0x02)
        skip = blen >= 3 ? 3 : blen;            /* 49 02 and the frame count */

    /* NULs removed; a non-ASCII byte becomes '?' (Python: U+FFFD); then stripped. */
    size_t n = 0;
    for (size_t i = skip; i < blen && n < OBD_VIN_MAX; i++) {
        if (body[i] == 0) continue;
        vin[n++] = body[i] < 0x80 ? (char)body[i] : '?';
    }
    vin[n] = '\0';
    size_t start = 0;
    while (start < n && isspace((unsigned char)vin[start])) start++;
    while (n > start && isspace((unsigned char)vin[n - 1])) n--;
    memmove(vin, vin + start, n - start);
    vin[n - start] = '\0';
    return vin[0] != '\0' ? OBD_OK : OBD_ERROR;       /* "empty VIN" */
}

void obd_vin_hash(const char *vin, char out[OBD_HASH_LEN + 1])
{
    uint8_t d[SHA256_DIGEST_SIZE];
    sha256(vin, strlen(vin), d);
    for (unsigned i = 0; i < OBD_HASH_LEN / 2; i++)
        snprintf(out + 2 * i, 3, "%02x", d[i]);
    out[OBD_HASH_LEN] = '\0';
}
