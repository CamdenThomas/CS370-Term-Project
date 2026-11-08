/* ELM327 reply text and Mode 01 / Mode 09 decoding: the C port of tools/obdlib.py.
 *
 * Nothing here talks to a port. It reads adapter text, decodes the standard Mode 01 PIDs
 * (SAE J1979 formulas), turns support bitmaps into PID sets and reads the VIN. It must
 * agree with obdlib.py line for line: the Forester's first capture was decoded by the
 * Python, and a C session of the same car must read the same values.
 *
 * Python raises; this returns a status. NoData -> OBD_NODATA, BusAsleep -> OBD_ASLEEP,
 * any other AdapterError -> OBD_ERROR, with the adapter's text in the answer's `text`.
 * Every parser takes a length, because a tty can deliver NUL bytes, which are skipped.
 */
#ifndef CARWATCH_PID_H
#define CARWATCH_PID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OBD_MAX_ECUS  8u    /* replies kept per request; more are counted, not kept */
#define OBD_MAX_DATA  16u   /* data bytes kept per reply line, after mode and PID */
#define OBD_TEXT_MAX  32u   /* the adapter's error text, NUL-terminated */
#define OBD_VIN_MAX   32u   /* a VIN is 17 characters; room for a malformed one */
#define OBD_HASH_LEN  12u   /* hex characters of the VIN hash, as obdlib.vin_hash */

typedef enum { OBD_OK = 0, OBD_NODATA, OBD_ASLEEP, OBD_ERROR } obd_status_t;

typedef struct {
    uint8_t len;                  /* bytes of data[] in use */
    uint8_t data[OBD_MAX_DATA];
} obd_line_t;

/* Every line that answered one request, in arrival order: one per ECU that replied. */
typedef struct {
    size_t     n;                 /* lines kept, at most OBD_MAX_ECUS */
    size_t     seen;              /* lines that answered, including any not kept */
    obd_line_t ecu[OBD_MAX_ECUS];
    char       text[OBD_TEXT_MAX];/* on failure: the adapter's text, or why we refused */
} obd_answer_t;

/* 256 bits, one per Mode 01 PID. */
typedef struct { uint8_t bits[32]; } obd_pidset_t;

static inline bool obd_pidset_has(const obd_pidset_t *s, uint8_t pid)
{
    return ((unsigned)s->bits[pid >> 3] >> (pid & 7u)) & 1u;
}
static inline void obd_pidset_add(obd_pidset_t *s, uint8_t pid)
{
    s->bits[pid >> 3] = (uint8_t)(s->bits[pid >> 3] | (1u << (pid & 7u)));
}

/* "010C" for (1, 0x0C). out must hold 5 bytes. */
void obd_request(char out[5], uint8_t mode, uint8_t pid);

/* obdlib.parse_all: the data of every line answering (mode, pid). OBD_NODATA for
 * `NO DATA`, OBD_ASLEEP for `UNABLE TO CONNECT` or `CAN ERROR`, OBD_ERROR for any other
 * failure, including a reply with no line for this request. The first failure line ends
 * the parse, as the Python's raise does. */
obd_status_t obd_parse_all(const char *raw, size_t len, uint8_t mode, uint8_t pid,
                           obd_answer_t *out);

/* obdlib.check_bus: OBD_ASLEEP if any line says the adapter could not reach the bus. */
obd_status_t obd_check_bus(const char *raw, size_t len);

/* obdlib.decode: true, with the value in the PID's unit, if the PID is in the table and
 * len is enough; false (value untouched) if unknown or short. */
bool obd_decode(uint8_t pid, const uint8_t *data, size_t len, double *value);

/* The PID's name and unit; "" for a PID not in the table. */
const char *obd_pid_name(uint8_t pid);
const char *obd_pid_unit(uint8_t pid);

/* True for every PID the decode table knows, so a test can walk it. */
bool obd_pid_known(uint8_t pid);

/* obdlib.support_pids: adds the PIDs a 4-byte support bitmap at base lists. -1 if len
 * is not 4. The most significant bit is base+1; the least, base+0x20, says the next
 * block exists. */
int obd_support_pids(uint8_t base, const uint8_t *data, size_t len, obd_pidset_t *set);

/* obdlib.support_from_reply: adds every PID any answering ECU lists. */
obd_status_t obd_support_from_reply(const char *raw, size_t len, uint8_t base,
                                    obd_pidset_t *set);

/* obdlib.decode_vin: the VIN from a Mode 09 PID 02 reply, multi-frame (`0: 49 02 01 ...`)
 * or one frame per line (`49 02 01 ...`). With two ECUs, the first's frames win. */
obd_status_t obd_decode_vin(const char *raw, size_t len, char vin[OBD_VIN_MAX + 1]);

/* obdlib.vin_hash: the first 12 hex digits of SHA-256(vin). Pseudonymization, not
 * anonymity: a VIN has little entropy (D-033). */
void obd_vin_hash(const char *vin, char out[OBD_HASH_LEN + 1]);

#endif
