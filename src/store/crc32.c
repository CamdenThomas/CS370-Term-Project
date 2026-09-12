#include "store/record.h"

/* CRC-32 (IEEE 802.3, reflected). Table built on first use — no static init cost in
 * the capture path. Replaced by a hardware CRC on the Pi only if it ever shows up in
 * a profile; do not optimize this before measuring (CLAUDE.md §7.6). */
static uint32_t g_table[256];
static int      g_ready;

static void build_table(void)
{
    for (uint32_t i = 0; i < 256u; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_table[i] = c;
    }
    g_ready = 1;
}

uint32_t rec_crc32(const void *buf, uint32_t len)
{
    if (!g_ready) build_table();
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++)
        c = g_table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
