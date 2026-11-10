/* Every event code in the product, in one table (D-042): the high byte names the sender,
 * the low byte the event. An event is one `key=value` line of at most 200 bytes, stored by
 * storaged as an event record (D-034) and also written to the sender's journal the moment
 * it happens. The names are DESIGN.md §4's log events.
 *
 * A code, once written to a log, keeps its meaning forever: retire codes, never reuse. */
#ifndef CARWATCH_EVENTS_H
#define CARWATCH_EVENTS_H

#define EV_SENDER_SUPERVISOR 0x01u
#define EV_SENDER_OBDD       0x02u
/*      0x03 was ambientd's, cut on 2026-10-28 with the probe; not reused. */
#define EV_SENDER_ANALYZED   0x04u
#define EV_SENDER_STORAGED   0x05u

#define EV_SENDER(code) ((unsigned)(code) >> 8)

/* obdd (Camden, 2026-11-08: the startup reads and a second ECU are events). */
#define EV_OBD_SESSION      0x0201u   /* adapter= protocol= voltage= vin_hash= supported= */
#define EV_OBD_STATIC_PIDS  0x0202u   /* the PIDs read once at start: 13=03 1C=03 51=01 */
#define EV_OBD_LINK_LOST    0x0203u
#define EV_OBD_RECONNECTED  0x0204u
#define EV_OBD_REPLY_BAD    0x0205u
#define EV_OBD_ADAPTER_RESET 0x0206u
#define EV_OBD_ECU_SILENT   0x0207u   /* the bus asleep: ignition off, key in ACC */
#define EV_OBD_ECU_AWAKE    0x0208u
#define EV_OBD_PID_ABSENT   0x0209u
#define EV_OBD_PID_PRESENT  0x020Au
#define EV_OBD_MIL_ON       0x020Bu
#define EV_OBD_MULTI_ECU    0x020Cu   /* a PID answered by more than one ECU; the first kept */
#define EV_OBD_REPLAY       0x020Du   /* obdd --replay: which capture, at what speed */
#define EV_OBD_SUPPORTED    0x020Eu   /* n= pids=01,03,...: what the ECUs list (43 on the
                                         Forester: too long to share the session event) */

/* storaged's own (wal.h names them for the write-ahead log). */
#define EV_STORE_RING_GEN   0x0501u   /* "ns=<0|1> gen=<hex>": which ring instance fed us */
#define EV_STORE_RING_SEQ   0x0502u   /* "ns=<0|1> seq=<hex>": last ring slot in this batch */

#endif
