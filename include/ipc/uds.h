/* The event path (D-042): every daemon sends its events to storaged over one Unix socket
 * of type SOCK_SEQPACKET, and storaged writes each as an event record in its next batch.
 * Samples travel by the ring; everything else that must be durable is an event.
 *
 * One packet is one event: code (u16, little-endian), src (u8, data_src_t), then up to 200
 * bytes of key=value text, no NUL. The src byte is an addition to D-042's draft, so an
 * event keeps its label like every record (CLAUDE.md §2.7).
 *
 * A sender never blocks and never dies for storaged: it keeps up to EV_QUEUE unsent events,
 * drops the oldest past that and counts the drops, and reconnects on its next send. Every
 * event is also a log line the moment it is made (ev_send logs it), so the journal is the
 * record of last resort. An event the socket accepted but storaged had not read when it
 * died is in the journal only.
 *
 * src/ipc/uds.c and src/interface/ are the only places a socket is opened (make boundary).
 */
#ifndef CARWATCH_UDS_H
#define CARWATCH_UDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EV_SOCK_DEFAULT  "/run/carwatch/events.sock"
#define EV_TEXT_MAX      200u   /* REC_EVENT_TEXT_MAX */
#define EV_PACKET_MAX    (3u + EV_TEXT_MAX)
#define EV_QUEUE         64u
#define EV_MAX_CLIENTS   8u     /* supervisor, obdd, analyzed, statusd, with room */
#define EV_PATH_MAX      108u   /* sun_path */

typedef struct {
    uint16_t code;
    uint8_t  src;
    uint8_t  len;                 /* bytes of text */
    char     text[EV_TEXT_MAX];   /* not NUL-terminated */
} ev_t;

typedef struct {
    char     path[EV_PATH_MAX];
    int      fd;                  /* -1 while not connected */
    bool     down_logged;         /* "storaged unreachable" said once per outage */
    ev_t     q[EV_QUEUE];
    size_t   head, n;             /* oldest unsent at q[head]; n queued */
    uint64_t sent, dropped;
} ev_sender_t;

void ev_sender_init(ev_sender_t *s, const char *path);
void ev_sender_close(ev_sender_t *s);

/* Logs the event, queues it, and tries to send everything queued. Text past 200 bytes is
 * cut. Never blocks. */
void ev_send(ev_sender_t *s, uint16_t code, uint8_t src, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* Tries to send what is queued; call it now and then so a backlog drains once storaged is
 * back. Returns the number still queued. */
size_t ev_flush(ev_sender_t *s);

typedef struct {
    char     path[EV_PATH_MAX];
    int      listen_fd;
    int      client[EV_MAX_CLIENTS];   /* -1 when free */
    uint64_t received, refused;
} ev_receiver_t;

/* Removes a stale socket at path, binds, sets mode 0600 and listens, non-blocking.
 * Returns 0, or -1 having logged why. */
int    ev_receiver_open(ev_receiver_t *r, const char *path);
void   ev_receiver_close(ev_receiver_t *r);

/* Accepts waiting senders, then reads up to max events into out without blocking.
 * A packet too short or without a valid src is refused and counted, never returned. */
size_t ev_receive(ev_receiver_t *r, ev_t *out, size_t max);

#endif
