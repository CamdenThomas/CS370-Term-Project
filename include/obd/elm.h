/* One session with an ELM327-compatible adapter over a tty: the C port of tools/elm.py.
 *
 * The adapter is /dev/obd on the Pi (an RFCOMM tty, D-019), or a pty from tools/obdsim.py
 * at a desk. Over Bluetooth the baud rate is ignored. One command is outstanding at a
 * time: write it with a CR, read up to the `>` prompt under a deadline.
 *
 * A request that times out may still be answered later. The session marks itself stale,
 * and the next command first waits for that late prompt (up to resync_ms), then, if none
 * comes, sends a bare CR, which makes a busy adapter stop (`STOPPED`, then `>`). A CR to
 * an idle adapter would repeat its last command, which is why it is not sent first.
 *
 * Every syscall failure is logged with errno here. The caller logs what it means
 * (`link_lost`, `reply_bad`, ...: DESIGN.md §4).
 */
#ifndef CARWATCH_ELM_H
#define CARWATCH_ELM_H

#include "obd/pid.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ELM_REPLY_MAX   1024u   /* bytes of one reply kept; more is read and dropped */
#define ELM_BANNER_MAX  64u
#define ELM_TIMEOUT_MS  2000    /* elm.py's default */
#define ELM_RESYNC_MS   500     /* elm.py's RESYNC_WAIT */

/* The first four are a reply's outcome and match obd_status_t; the last two are the
 * link's. elm.py: NoData, BusAsleep, AdapterError, Timeout, LinkLost. */
typedef enum {
    ELM_OK        = OBD_OK,
    ELM_NODATA    = OBD_NODATA,
    ELM_ASLEEP    = OBD_ASLEEP,
    ELM_ERROR     = OBD_ERROR,
    ELM_TIMEOUT,                 /* no prompt by the deadline; the session is now stale */
    ELM_LINK_LOST                /* the tty is gone: Bluetooth dropped, adapter off */
} elm_status_t;

typedef struct {
    int    fd;                         /* -1 when closed */
    int    timeout_ms;                 /* per command, unless the call gives its own */
    int    resync_ms;
    bool   stale;                      /* a request timed out; its reply may be coming */
    char   banner[ELM_BANNER_MAX];     /* the reset banner, from elm_init */
    char   reply[ELM_REPLY_MAX + 1];   /* the last command's reply text, NUL-terminated */
    size_t reply_len;                  /* bytes in reply[], which may hold NULs */
    bool   reply_cut;                  /* the reply was longer than ELM_REPLY_MAX */
} elm_t;

/* Opens path raw (O_NOCTTY, non-blocking, close-on-exec). ELM_OK or ELM_LINK_LOST. */
elm_status_t elm_open(elm_t *e, const char *path);
void         elm_close(elm_t *e);

/* Sends one command; its reply is in e->reply. timeout_ms 0 uses e->timeout_ms.
 * ELM_OK, ELM_TIMEOUT or ELM_LINK_LOST: the reply's text is not judged here. */
elm_status_t elm_command(elm_t *e, const char *cmd, int timeout_ms);

/* ATZ (5 s), echo off, linefeeds off, spaces on, headers off, auto protocol, then 0100
 * (15 s: the protocol search). ELM_ASLEEP when the bus is off (ignition off): the caller
 * waits and tries again, since a reset will not help. */
elm_status_t elm_init(elm_t *e);

/* True when the banner says SIM: tools/obdsim.py, whose data is synth (CLAUDE.md §2.7). */
bool elm_is_sim(const elm_t *e);

/* Every line that answered one request, one per ECU. */
elm_status_t elm_query(elm_t *e, uint8_t mode, uint8_t pid, obd_answer_t *out);

/* Every Mode 01 PID the ECUs list, walking 0x00, 0x20, ... while chained. ELM_NODATA if
 * nothing answers 0100. */
elm_status_t elm_supported(elm_t *e, obd_pidset_t *out);

/* ATDP (protocol) or ATRV (battery voltage): the reply's lines joined by spaces. */
elm_status_t elm_info(elm_t *e, const char *cmd, char *out, size_t cap);

/* 0902 (5 s): the VIN. */
elm_status_t elm_vin(elm_t *e, char vin[OBD_VIN_MAX + 1]);

/* elm.reply_lines: the reply's non-empty lines, without the prompt, joined by spaces;
 * a line equal to skip (the echo of a command, or NULL) is left out. */
void elm_join_lines(const char *raw, size_t len, const char *skip, char *out, size_t cap);

#endif
