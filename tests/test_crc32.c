#include "store/record.h"

#include <stdio.h>
#include <string.h>

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

    return failures == 0 ? 0 : 1;
}
