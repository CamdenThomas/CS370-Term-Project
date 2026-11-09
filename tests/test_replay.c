/* src/obd/replay.c: reading obdlog's lines, and the label a replay carries. */
#include "common/log.h"
#include "obd/replay.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

/* Lines as obdlog writes them (captures/forester/20261028T220659.957/drive.jsonl). */
static const char *k_header =
    "{\"type\":\"header\",\"src\":\"live\",\"seq\":0,\"t_mono\":60924.8626,"
    "\"adapter\":\"ELM327 v1.4b\",\"protocol\":\"AUTO, ISO 15765-4 (CAN 11/500)\","
    "\"vin_hash\":\"16d15766b381\",\"pids\":[\"01\",\"03\"]}";
static const char *k_sample =
    "{\"type\":\"sample\",\"src\":\"live\",\"seq\":4,\"t_mono\":60925.0278,"
    "\"pid\":\"0C\",\"raw\":\"0A 10\",\"val\":644.0,\"unit\":\"rpm\",\"n_ecu\":1}";
static const char *k_null = "{\"type\":\"sample\",\"pid\":\"01\",\"raw\":\"00 07 E5 00\","
                            "\"val\":null}";

int main(void)
{
    char s[64];
    double d;

    CHECK(replay_str(k_header, "type", s, sizeof s) && strcmp(s, "header") == 0, "type");
    CHECK(replay_str(k_header, "protocol", s, sizeof s) &&
          strcmp(s, "AUTO, ISO 15765-4 (CAN 11/500)") == 0, "a value with commas and spaces");
    CHECK(replay_num(k_header, "t_mono", &d) && d == 60924.8626, "a number");
    CHECK(!replay_str(k_header, "pid", s, sizeof s), "\"pid\" does not match \"pids\"");
    CHECK(!replay_str(k_header, "missing", s, sizeof s), "an absent key is false");
    CHECK(!replay_str(k_header, "adapter", s, 4), "a value too long for the buffer is false");

    CHECK(replay_str(k_sample, "pid", s, sizeof s) && strcmp(s, "0C") == 0, "pid");
    CHECK(replay_str(k_sample, "raw", s, sizeof s) && strcmp(s, "0A 10") == 0, "raw");
    CHECK(replay_num(k_sample, "val", &d) && d == 644.0, "val");
    CHECK(!replay_num(k_null, "val", &d), "null is not a number");

    /* A key's name inside a string value is not the key. */
    const char *tricky = "{\"note\":\"pid\",\"pid\":\"05\"}";
    CHECK(replay_str(tricky, "pid", s, sizeof s) && strcmp(s, "05") == 0,
          "a key, not a string that spells it");
    const char *esc = "{\"a\":\"x\\\"y\\\\z\",\"b\":\"q\\n\"}";
    CHECK(replay_str(esc, "a", s, sizeof s) && strcmp(s, "x\"y\\z") == 0, "\\\" and \\\\");
    CHECK(!replay_str(esc, "b", s, sizeof s), "other escapes are refused");
    CHECK(!replay_str("{\"a\":\"open", "a", s, sizeof s), "an unterminated string is false");

    obd_line_t l;
    CHECK(replay_raw("0A 10", &l) && l.len == 2 && l.data[0] == 0x0A && l.data[1] == 0x10,
          "raw hex");
    CHECK(replay_raw("00 07 e5 00", &l) && l.len == 4 && l.data[2] == 0xE5, "lower case");
    CHECK(!replay_raw("0A 1", &l) && !replay_raw("ZZ", &l) && !replay_raw("", &l),
          "odd, non-hex and empty are refused");

    CHECK(replay_label("live") == SRC_REPLAY, "a live capture replayed is replay");
    CHECK(replay_label("replay") == SRC_REPLAY, "a replay of a replay is replay");
    CHECK(replay_label("synth") == SRC_SYNTH, "synth stays synth");
    CHECK(replay_label("") == SRC_INVALID && replay_label("LIVE") == SRC_INVALID,
          "anything else is refused");
    return failures == 0 ? 0 : 1;
}
