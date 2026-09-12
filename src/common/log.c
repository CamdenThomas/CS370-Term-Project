#include "common/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char  *g_component = "?";
static log_level_t  g_min       = LOG_INFO;
static data_src_t   g_src       = SRC_LIVE;

static const char *level_name(log_level_t l)
{
    switch (l) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO";
    case LOG_WARN:  return "WARN";
    case LOG_ERROR: return "ERROR";
    case LOG_FATAL: return "FATAL";
    }
    return "?";
}

void log_init(const char *component, log_level_t min)
{
    g_component = component;
    g_min       = min;
    setvbuf(stdout, NULL, _IOLBF, 0);
}

void log_set_src(data_src_t src) { g_src = src; }
data_src_t log_get_src(void)     { return g_src; }

const char *log_src_name(data_src_t src)
{
    switch (src) {
    case SRC_LIVE:   return "live";
    case SRC_REPLAY: return "replay";
    case SRC_SYNTH:  return "synth";
    }
    return "unknown";
}

static void stamp(char *buf, size_t n)
{
    struct timespec ts;
    struct tm       tm;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0 ||
        gmtime_r(&ts.tv_sec, &tm) == NULL) {
        snprintf(buf, n, "----------T--:--:--.---Z");
        return;
    }
    size_t k = strftime(buf, n, "%Y-%m-%dT%H:%M:%S", &tm);
    snprintf(buf + k, n - k, ".%03ldZ", ts.tv_nsec / 1000000L);
}

void log_emit(log_level_t lvl, const char *fmt, ...)
{
    if (lvl < g_min) return;

    char ts[40];
    stamp(ts, sizeof ts);

    /* ts level component src pid — then the message. Greppable and column-stable. */
    printf("%s %-5s %-10s src=%-6s pid=%d ",
           ts, level_name(lvl), g_component, log_src_name(g_src), (int)getpid());

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    putchar('\n');
}

/* Resident set size in KiB, from /proc/self/statm field 2 (pages). */
static long rss_kib(void)
{
    FILE *f = fopen("/proc/self/statm", "re");
    if (f == NULL) return -1;
    long total = 0, res = 0;
    int  got = fscanf(f, "%ld %ld", &total, &res);
    (void)fclose(f);
    if (got != 2) return -1;
    long page_kib = sysconf(_SC_PAGESIZE) / 1024;
    return res * page_kib;
}

void log_heartbeat(uint64_t events, uint64_t drops)
{
    log_emit(LOG_INFO, "heartbeat rss_kib=%ld events=%llu drops=%llu",
             rss_kib(), (unsigned long long)events, (unsigned long long)drops);
}
