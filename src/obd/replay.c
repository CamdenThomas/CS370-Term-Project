/* Reading obdlog captures back (include/obd/replay.h). */
#include "obd/replay.h"

#include "common/log.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Points just past `"key":` and any spaces, or NULL. Only a key, never a value: a key is
 * preceded by `{` or `,` (and spaces), so "pid" never matches inside "pids" or a string. */
static const char *value_of(const char *line, const char *key)
{
    size_t k = strlen(key);
    for (const char *p = strchr(line, '"'); p != NULL; p = strchr(p + 1, '"')) {
        if (strncmp(p + 1, key, k) != 0 || p[1 + k] != '"') continue;
        const char *b = p;
        while (b > line && isspace((unsigned char)b[-1])) b--;
        if (b != line && b[-1] != '{' && b[-1] != ',') continue;
        const char *v = p + 2 + k;
        while (isspace((unsigned char)*v)) v++;
        if (*v != ':') continue;
        v++;
        while (isspace((unsigned char)*v)) v++;
        return v;
    }
    return NULL;
}

bool replay_str(const char *line, const char *key, char *out, size_t cap)
{
    const char *v = value_of(line, key);
    if (v == NULL || *v != '"' || cap == 0) return false;
    size_t n = 0;
    for (v++; *v != '"'; v++) {
        if (*v == '\0') return false;                    /* unterminated */
        if (*v == '\\') {
            v++;
            if (*v != '"' && *v != '\\') return false;  /* \n, \u...: not in these fields */
        }
        if (n + 1 >= cap) return false;
        out[n++] = *v;
    }
    out[n] = '\0';
    return true;
}

bool replay_num(const char *line, const char *key, double *out)
{
    const char *v = value_of(line, key);
    if (v == NULL || !(*v == '-' || isdigit((unsigned char)*v))) return false;
    char *end;
    double d = strtod(v, &end);
    if (end == v || !(*end == ',' || *end == '}' || isspace((unsigned char)*end))) return false;
    *out = d;
    return true;
}

uint8_t replay_label(const char *src)
{
    if (strcmp(src, "live") == 0 || strcmp(src, "replay") == 0) return SRC_REPLAY;
    if (strcmp(src, "synth") == 0) return SRC_SYNTH;
    return SRC_INVALID;
}

static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool replay_raw(const char *hex, obd_line_t *out)
{
    memset(out, 0, sizeof *out);
    int hi = -1;
    for (; *hex != '\0'; hex++) {
        if (*hex == ' ') continue;
        int v = nibble(*hex);
        if (v < 0) return false;
        if (hi < 0) { hi = v; continue; }
        if (out->len == OBD_MAX_DATA) return false;
        out->data[out->len++] = (uint8_t)(hi << 4 | v);
        hi = -1;
    }
    return hi < 0 && out->len > 0;
}
