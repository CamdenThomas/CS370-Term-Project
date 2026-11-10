/* SHA-256 against the examples FIPS 180-2 publishes. If one of these changes, every VIN
 * hash obdd writes stops matching the ones tools/obdlib.py wrote for the same car. */
#include "common/sha256.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void hex(const uint8_t d[SHA256_DIGEST_SIZE], char out[2 * SHA256_DIGEST_SIZE + 1])
{
    for (unsigned i = 0; i < SHA256_DIGEST_SIZE; i++)
        snprintf(out + 2 * i, 3, "%02x", d[i]);
}

static void check(const char *what, const uint8_t d[SHA256_DIGEST_SIZE], const char *want)
{
    char got[2 * SHA256_DIGEST_SIZE + 1];
    hex(d, got);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "  FAIL sha256(%s) = %s, want %s\n", what, got, want);
        failures++;
    }
}

int main(void)
{
    uint8_t d[SHA256_DIGEST_SIZE];

    sha256("abc", 3, d);
    check("\"abc\"", d, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    sha256("", 0, d);
    check("\"\"", d, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    /* 448 bits: the padding no longer fits in the last block, so a second one is made. */
    /* cspell:disable-next-line */
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha256(two, strlen(two), d);
    check("448-bit message", d,
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    /* A million 'a', fed in uneven pieces, so updates straddle block boundaries. */
    sha256_t s;
    sha256_init(&s);
    char a[997];
    memset(a, 'a', sizeof a);
    size_t left = 1000000;
    while (left > 0) {
        size_t n = left < sizeof a ? left : sizeof a;
        sha256_update(&s, a, n);
        left -= n;
    }
    sha256_final(&s, d);
    check("a million 'a'", d,
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    return failures == 0 ? 0 : 1;
}
