/* src/obd/pid.c against tests/tools/test_obdlib.py, case for case: the C decoder must
 * read a reply exactly as the Python logger that made the first Forester capture did. */
#include "obd/pid.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static obd_status_t parse(const char *raw, uint8_t pid, obd_answer_t *a)
{
    return obd_parse_all(raw, strlen(raw), 0x01, pid, a);
}

static bool data_is(const obd_answer_t *a, size_t i, const uint8_t *want, size_t n)
{
    return a->n > i && a->ecu[i].len == n && memcmp(a->ecu[i].data, want, n) == 0;
}

/* --- ParseResponse ---------------------------------------------------------------------- */

static void test_parse_response(void)
{
    obd_answer_t a;
    static const uint8_t rpm[] = { 0x1A, 0xF8 };

    CHECK(parse("41 0C 1A F8 \r\r>", 0x0C, &a) == OBD_OK && data_is(&a, 0, rpm, 2),
          "single-line reply returns the data after the PID");
    CHECK(parse("410C1AF8\r\r>", 0x0C, &a) == OBD_OK && data_is(&a, 0, rpm, 2),
          "spaces-off reply");

    static const uint8_t spd[] = { 0x3C };
    CHECK(parse("SEARCHING...\r41 0D 3C \r\r>", 0x0D, &a) == OBD_OK && data_is(&a, 0, spd, 1),
          "SEARCHING... is skipped");

    static const uint8_t c5a[] = { 0x5A }, c5b[] = { 0x5B };
    CHECK(parse("41 05 5A \r41 05 5B \r\r>", 0x05, &a) == OBD_OK && a.n == 2 &&
          data_is(&a, 0, c5a, 1) && data_is(&a, 1, c5b, 1),
          "every ECU's line is kept, the first first");

    CHECK(parse("41 0D 3C \r\r>", 0x0C, &a) == OBD_ERROR, "another PID's reply is refused");

    CHECK(parse("BUS INIT: ...OK\r41 0C 1A F8 \r\r>", 0x0C, &a) == OBD_OK &&
          data_is(&a, 0, rpm, 2), "BUS INIT: ...OK is skipped");
    CHECK(parse("BUS INIT: ...ERROR\r\r>", 0x0C, &a) == OBD_ERROR &&
          strcmp(a.text, "BUS INIT: ...ERROR") == 0, "BUS INIT: ...ERROR fails with its text");

    CHECK(parse("NO DATA\r\r>", 0x5C, &a) == OBD_NODATA, "NO DATA is its own status");

    static const char *const errs[] = { "?", "BUFFER FULL", "STOPPED", "BUS BUSY",
                                        "<DATA ERROR" };
    for (size_t i = 0; i < sizeof errs / sizeof errs[0]; i++) {
        char raw[64];
        snprintf(raw, sizeof raw, "%s\r\r>", errs[i]);
        CHECK(parse(raw, 0x0C, &a) == OBD_ERROR, errs[i]);
    }

    CHECK(parse("UNABLE TO CONNECT\r\r>", 0x0C, &a) == OBD_ASLEEP, "no bus: asleep");
    CHECK(parse("CAN ERROR\r\r>", 0x0C, &a) == OBD_ASLEEP, "CAN ERROR: asleep");
    const char *asleep = "SEARCHING...\rUNABLE TO CONNECT\r\r>";
    CHECK(obd_check_bus(asleep, strlen(asleep)) == OBD_ASLEEP, "check_bus sees no bus");
    const char *awake = "SEARCHING...\r41 00 BE 1F A8 13 \r\r>";
    CHECK(obd_check_bus(awake, strlen(awake)) == OBD_OK, "check_bus passes a reply");

    const char nul[] = "\0" "41 0C 1A F8 \0\r\r>";
    CHECK(obd_parse_all(nul, sizeof nul - 1, 0x01, 0x0C, &a) == OBD_OK &&
          data_is(&a, 0, rpm, 2), "NUL bytes from the adapter are ignored");

    CHECK(parse("\r>", 0x0C, &a) == OBD_ERROR && strcmp(a.text, "empty reply") == 0,
          "an empty reply fails");
    CHECK(parse("41 0c 1a f8\r>", 0x0C, &a) == OBD_OK && data_is(&a, 0, rpm, 2),
          "lower-case hex is read");
}

/* --- Decode ----------------------------------------------------------------------------- */

static bool decodes(uint8_t pid, const uint8_t *d, size_t n, double want)
{
    double v = -12345.0;
    return obd_decode(pid, d, n, &v) && fabs(v - want) < 1e-9;
}

static void test_decode(void)
{
    static const uint8_t rpm[] = { 0x1A, 0xF8 };
    CHECK(decodes(0x0C, rpm, 2, 1726.0) && strcmp(obd_pid_unit(0x0C), "rpm") == 0, "rpm");
    static const uint8_t spd[] = { 0x3C };
    CHECK(decodes(0x0D, spd, 1, 60.0) && strcmp(obd_pid_unit(0x0D), "km/h") == 0, "speed");
    static const uint8_t c[] = { 0x5A };
    CHECK(decodes(0x05, c, 1, 50.0) && strcmp(obd_pid_unit(0x05), "degC") == 0,
          "coolant has a forty-degree offset");
    static const uint8_t mid[] = { 128 }, zero[] = { 0 };
    CHECK(decodes(0x06, mid, 1, 0.0) && decodes(0x06, zero, 1, -100.0) &&
          strcmp(obd_pid_unit(0x06), "%") == 0, "fuel trim is centered on 128");
    static const uint8_t full[] = { 255 };
    CHECK(decodes(0x04, full, 1, 100.0), "load scales 255 to 100 percent");
    static const uint8_t maf[] = { 0x01, 0x90 };
    CHECK(decodes(0x10, maf, 2, 4.0), "maf");
    static const uint8_t o2[] = { 0x5A, 0xFF };
    CHECK(decodes(0x14, o2, 2, 0.45) && strcmp(obd_pid_unit(0x14), "V") == 0,
          "narrow-band O2 reports the voltage");

    double v = 7.0;
    static const uint8_t two[] = { 1, 2 };
    CHECK(!obd_decode(0xFE, two, 2, &v) && v == 7.0 && obd_pid_name(0xFE)[0] == '\0',
          "an unknown PID does not decode");
    static const uint8_t one[] = { 0x1A };
    CHECK(!obd_decode(0x0C, one, 1, &v) && v == 7.0, "short data does not decode");

    for (unsigned p = 0; p < 256; p++)
        if (obd_pid_known((uint8_t)p))
            CHECK(obd_pid_name((uint8_t)p)[0] != '\0', "every listed PID has a name");
    CHECK(strcmp(obd_pid_name(0x24), "O2 sensor 1 lambda") == 0 &&
          strcmp(obd_pid_name(0x3E), "Catalyst temperature B1S2") == 0,
          "generated names match obdlib's");

    /* Bit-for-bit with Python for a value that does not divide evenly. */
    static const uint8_t load[] = { 0x8C };
    CHECK(obd_decode(0x04, load, 1, &v) && v == 140 * 100.0 / 255.0,
          "load 0x8C is Python's 54.90196078431372");
}

/* --- SupportBitmap ---------------------------------------------------------------------- */

static void test_support(void)
{
    obd_pidset_t s;
    memset(&s, 0, sizeof s);
    static const uint8_t j1979[] = { 0xBE, 0x1F, 0xA8, 0x13 };   /* SAE J1979's example */
    CHECK(obd_support_pids(0x00, j1979, 4, &s) == 0, "a 4-byte bitmap parses");
    CHECK(obd_pidset_has(&s, 0x01), "MSB is the first PID of the block");
    CHECK(!obd_pidset_has(&s, 0x02), "0x02 is not supported");
    CHECK(obd_pidset_has(&s, 0x0C), "0x0C is supported");
    CHECK(obd_pidset_has(&s, 0x20), "the next-block bit is listed");
    CHECK(!obd_pidset_has(&s, 0x21), "nothing past the block");

    memset(&s, 0, sizeof s);
    static const uint8_t one[] = { 0x80, 0, 0, 0 };
    obd_support_pids(0x20, one, 4, &s);
    unsigned count = 0;
    for (unsigned p = 0; p < 256; p++) count += obd_pidset_has(&s, (uint8_t)p);
    CHECK(count == 1 && obd_pidset_has(&s, 0x21), "offset by the block base");

    memset(&s, 0, sizeof s);
    static const uint8_t all[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    obd_support_pids(0xE0, all, 4, &s);
    CHECK(obd_pidset_has(&s, 0xFF) && !obd_pidset_has(&s, 0x00),
          "the 0xE0 block's next-block bit does not wrap to PID 0");

    memset(&s, 0, sizeof s);
    const char *two = "41 00 80 00 00 00 \r41 00 BE 1F A8 13 \r\r>";
    CHECK(obd_support_from_reply(two, strlen(two), 0x00, &s) == OBD_OK &&
          obd_pidset_has(&s, 0x01) && obd_pidset_has(&s, 0x0C) && obd_pidset_has(&s, 0x20),
          "bitmaps from every answering ECU are combined");

    static const uint8_t three[] = { 1, 2, 3 };
    CHECK(obd_support_pids(0x00, three, 3, &s) == -1, "a wrong length is an error");
}

/* --- Vin -------------------------------------------------------------------------------- */

#define MULTI "014\r0: 49 02 01 31 48 47\r1: 43 4D 38 32 36 33 33\r2: 41 30 30 34 33 35 32\r"

static void test_vin(void)
{
    char vin[OBD_VIN_MAX + 1];
    const char *multi = MULTI "\r>";
    CHECK(obd_decode_vin(multi, strlen(multi), vin) == OBD_OK &&
          strcmp(vin, "1HGCM82633A004352") == 0, "multi-frame VIN");

    const char *single = "49 02 01 00 00 00 31\r49 02 02 48 47 43 4D\r49 02 03 38 32 36 33\r"
                         "49 02 04 33 41 30 30\r49 02 05 34 33 35 32\r\r>";
    CHECK(obd_decode_vin(single, strlen(single), vin) == OBD_OK &&
          strcmp(vin, "1HGCM82633A004352") == 0, "one frame per line VIN");

    const char *second = MULTI "014\r0: 49 02 01 32 32 32\r1: 32 32 32 32 32 32 32\r"
                         "2: 32 32 32 32 32 32 32\r\r>";
    CHECK(obd_decode_vin(second, strlen(second), vin) == OBD_OK &&
          strcmp(vin, "1HGCM82633A004352") == 0, "a second ECU's frames do not overwrite");

    CHECK(obd_decode_vin("NO DATA\r\r>", 10, vin) == OBD_NODATA, "NO DATA means no VIN");

    char h1[OBD_HASH_LEN + 1], h2[OBD_HASH_LEN + 1], h3[OBD_HASH_LEN + 1];
    obd_vin_hash("1HGCM82633A004352", h1);
    obd_vin_hash("1HGCM82633A004352", h2);
    obd_vin_hash("1HGCM82633A004353", h3);
    CHECK(strcmp(h1, h2) == 0 && strlen(h1) == 12 && strstr(h1, "1HGCM") == NULL &&
          strcmp(h1, h3) != 0, "the hash is stable, short and hides the VIN");

    /* obdsim's VIN, hashed by obdlib.vin_hash: fixtures/synth/drive-cycle-synth.jsonl. */
    obd_vin_hash("SIMCARWATCH000001", h1);
    CHECK(strcmp(h1, "bb984ebda934") == 0, "the same hash as tools/obdlib.py");
}

static void test_request(void)
{
    char r[5];
    obd_request(r, 0x01, 0x0C);
    CHECK(strcmp(r, "010C") == 0, "010C");
    obd_request(r, 0x09, 0x02);
    CHECK(strcmp(r, "0902") == 0, "0902");
}

int main(void)
{
    test_parse_response();
    test_decode();
    test_support();
    test_vin();
    test_request();
    return failures == 0 ? 0 : 1;
}
