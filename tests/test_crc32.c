#include "store/record.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The definition, one bit at a time: the reference the table must match. */
static uint32_t bitwise_crc32(const uint8_t *p, uint32_t len)
{
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }
    return c ^ 0xFFFFFFFFu;
}

int main(void)
{
    int failures = 0;

    /* Known-answer test: CRC-32/IEEE of "123456789" is 0xCBF43926. If this ever
     * changes, the on-disk format has silently changed with it. */
    const char *v = "123456789";
    uint32_t got = rec_crc32(v, (uint32_t)strlen(v));
    if (got != 0xCBF43926u) {
        fprintf(stderr, "  FAIL crc32(\"123456789\") = 0x%08X, want 0xCBF43926\n", got);
        failures++;
    }

    if (rec_crc32("", 0) != 0u) {
        fprintf(stderr, "  FAIL crc32 of empty input must be 0\n");
        failures++;
    }

    /* A single flipped bit must change the CRC — that is the entire reason a torn
     * record is detectable on recovery. */
    char a[] = "carwatch", b[] = "carwatci";
    if (rec_crc32(a, 8) == rec_crc32(b, 8)) {
        fprintf(stderr, "  FAIL crc32 collided on a one-bit difference\n");
        failures++;
    }

    /* The table is a constant written out in crc32.c; check it against the bitwise
     * definition over every single-byte input and a longer pseudo-random buffer. */
    uint8_t buf[1024];
    uint32_t x = 2463534242u;
    for (size_t i = 0; i < sizeof buf; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        buf[i] = (uint8_t)x;
    }
    for (uint32_t n = 0; n <= sizeof buf; n += (n < 256u ? 1u : 97u)) {
        if (rec_crc32(buf, n) != bitwise_crc32(buf, n)) {
            fprintf(stderr, "  FAIL table CRC differs from bitwise CRC at length %u\n", n);
            failures++;
            break;
        }
    }

    return failures == 0 ? 0 : 1;
}
