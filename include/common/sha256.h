/* SHA-256 (FIPS 180-4). obdd uses it only to hash the VIN: a short stable label that
 * groups sessions by car without writing the VIN down (D-033), and it must give the same
 * label as tools/obdlib.py's vin_hash so a C session and a Python one of the same car
 * agree. Not used for anything that needs to be secret. */
#ifndef CARWATCH_SHA256_H
#define CARWATCH_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32u

typedef struct {
    uint32_t h[8];
    uint64_t bytes;      /* message length so far */
    uint8_t  block[64];
    size_t   used;       /* bytes of block[] filled */
} sha256_t;

void sha256_init(sha256_t *s);
void sha256_update(sha256_t *s, const void *data, size_t len);
void sha256_final(sha256_t *s, uint8_t out[SHA256_DIGEST_SIZE]);

/* One-shot: the digest of data[0, len). */
void sha256(const void *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]);

#endif
