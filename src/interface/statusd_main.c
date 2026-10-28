/* statusd: the phone page and API (D-013, D-043, D-045).
 *
 * A supervisable daemon (CLAUDE.md section 2.6): it exits 0 on SIGTERM or SIGINT, 1 when it
 * cannot run, 2 on bad arguments; it holds no descriptor but its listener (close-on-exec);
 * it logs a heartbeat at once and then every --heartbeat-s seconds; and nothing it does
 * depends on a sibling being alive, so a kill -9 of any other daemon changes nothing here.
 *
 * It serves only the box's own access-point address, never every interface: --bind has no
 * default but loopback. Until the real source exists it answers from the synthetic one,
 * and says so on every log line (src=synth).
 */
#include "common/log.h"
#include "interface/route.h"
#include "interface/server.h"
#include "interface/synth_source.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_BIND         "127.0.0.1"
#define DEFAULT_PORT         8080u
#define DEFAULT_HEARTBEAT_S  3600u
#define MAX_HEARTBEAT_S      86400u

typedef struct {
    const char *bind;
    uint16_t    port;
    unsigned    heartbeat_s;
} args_t;

typedef enum { ARGS_OK, ARGS_HELP, ARGS_USAGE } args_result_t;

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
    g_stop = sig;
}

/* ------------------------------------------------------------------ arguments */

static void usage(FILE *to)
{
    fprintf(to,
            "usage: statusd [--bind ADDR] [--port N] [--heartbeat-s N]\n"
            "  --bind ADDR       IPv4 address to serve on (default " DEFAULT_BIND ")\n"
            "  --port N          TCP port, 0 for any free one (default %u)\n"
            "  --heartbeat-s N   seconds between heartbeat log lines, 1 to %u (default %u)\n"
            "exit status: 0 stopped by SIGTERM or SIGINT, 1 could not run, 2 bad arguments\n",
            DEFAULT_PORT, MAX_HEARTBEAT_S, DEFAULT_HEARTBEAT_S);
}

/* s is wholly decimal digits and at most max. */
static bool parse_number(const char *s, unsigned long max, unsigned long *out)
{
    char *end;

    if (s[0] < '0' || s[0] > '9') return false;     /* no sign, no space, not empty */
    errno = 0;
    *out = strtoul(s, &end, 10);
    return errno == 0 && *end == '\0' && *out <= max;
}

static args_result_t parse_args(int argc, char **argv, args_t *a)
{
    a->bind = DEFAULT_BIND;
    a->port = (uint16_t)DEFAULT_PORT;
    a->heartbeat_s = DEFAULT_HEARTBEAT_S;

    for (int i = 1; i < argc; i++) {
        const char *flag = argv[i];
        unsigned long n;

        if (strcmp(flag, "--help") == 0 || strcmp(flag, "-h") == 0) {
            usage(stdout);
            return ARGS_HELP;
        }
        if (i + 1 >= argc || (strcmp(flag, "--bind") != 0 && strcmp(flag, "--port") != 0 &&
                              strcmp(flag, "--heartbeat-s") != 0)) {
            fprintf(stderr, "statusd: %s: %s\n", flag,
                    i + 1 >= argc && strncmp(flag, "--", 2) == 0 ? "needs a value" : "unknown argument");
            usage(stderr);
            return ARGS_USAGE;
        }
        const char *value = argv[++i];

        if (strcmp(flag, "--bind") == 0) {
            struct in_addr probe;
            if (inet_pton(AF_INET, value, &probe) != 1) {
                fprintf(stderr, "statusd: --bind: %s is not an IPv4 address\n", value);
                return ARGS_USAGE;
            }
            a->bind = value;
        } else if (strcmp(flag, "--port") == 0) {
            if (!parse_number(value, 65535, &n)) {
                fprintf(stderr, "statusd: --port: %s is not 0 to 65535\n", value);
                return ARGS_USAGE;
            }
            a->port = (uint16_t)n;
        } else {
            if (!parse_number(value, MAX_HEARTBEAT_S, &n) || n == 0) {
                fprintf(stderr, "statusd: --heartbeat-s: %s is not 1 to %u\n", value, MAX_HEARTBEAT_S);
                return ARGS_USAGE;
            }
            a->heartbeat_s = (unsigned)n;
        }
    }
    return ARGS_OK;
}

/* ---------------------------------------------------------------------- clocks */

static uint64_t wall_seconds(void)
{
    static uint64_t last;
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) LOG_E("clock_gettime: %s", strerror(errno));
    else last = (uint64_t)ts.tv_sec;
    return last;
}

static uint64_t mono_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        LOG_E("clock_gettime: %s", strerror(errno));
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* ----------------------------------------------------------------- the source */

/* The synthetic source, moved to the real time before each request, so that its samples
 * are from now and keep moving. The real source (a client of ctl.sock) replaces this whole
 * struct, and route.c never knows which it has. */
typedef struct {
    synth_t  synth;
    source_t inner;
    source_t outer;
} live_source_t;

static ssize_t clocked_query(void *ctx, const char *request, char *out, size_t cap)
{
    live_source_t *l = ctx;

    synth_set_time(&l->synth, wall_seconds());
    return source_query(&l->inner, request, out, cap);
}

/* ---------------------------------------------------------------------- setup */

static int install_signals(void)
{
    struct sigaction stop = { .sa_handler = on_signal };   /* no SA_RESTART: poll must return */
    struct sigaction ignore = { .sa_handler = SIG_IGN };

    sigemptyset(&stop.sa_mask);
    sigemptyset(&ignore.sa_mask);
    if (sigaction(SIGTERM, &stop, NULL) < 0 || sigaction(SIGINT, &stop, NULL) < 0) {
        LOG_E("sigaction: %s", strerror(errno));
        return -1;
    }
    if (sigaction(SIGPIPE, &ignore, NULL) < 0) {
        LOG_E("sigaction SIGPIPE: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/* About 280 KB between them, so they live in static storage, not on the stack. */
static server_t      srv;
static route_ctx_t   rc;
static live_source_t live;

int main(int argc, char **argv)
{
    args_t args;
    int status = 0;

    switch (parse_args(argc, argv, &args)) {
    case ARGS_OK:    break;
    case ARGS_HELP:  return 0;
    case ARGS_USAGE: return 2;
    }

    log_init("statusd", LOG_INFO);
    log_set_src(SRC_SYNTH);             /* every line says so, while the data is made up */

    if (install_signals() < 0) return 1;

    synth_init(&live.synth, wall_seconds());
    live.inner = synth_source(&live.synth);
    live.outer = (source_t){ .query = clocked_query, .ctx = &live };
    rc.src = &live.outer;

    if (server_open(&srv, args.bind, args.port, route_handle, &rc) < 0) return 1;

    uint64_t every_ms = (uint64_t)args.heartbeat_s * 1000u;
    uint64_t next_heartbeat = mono_ms() + every_ms;

    log_heartbeat(server_served(&srv), server_drops(&srv));   /* alive, from the first second */
    while (!g_stop) {
        if (server_step(&srv, 1000) < 0) {
            LOG_E("the server failed; exiting");
            status = 1;
            break;
        }
        if (mono_ms() >= next_heartbeat) {
            log_heartbeat(server_served(&srv), server_drops(&srv));
            next_heartbeat = mono_ms() + every_ms;
        }
    }

    if (g_stop) LOG_I("signal %d: stopping", (int)g_stop);
    server_close(&srv);
    return status;
}
