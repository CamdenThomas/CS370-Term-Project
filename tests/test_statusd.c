/* statusd as a process (CLAUDE.md section 2.6): started, queried over a real socket, sent
 * signals, and judged from outside by its exit codes, its log, its descriptors and the
 * port it leaves behind. The binary is the one next to this test, so `make asan` runs the
 * sanitized build. Every wait has a timeout, and a child is always killed on the way out. */
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

/* ------------------------------------------------------------------ processes */

typedef struct {
    pid_t  pid;           /* 0 once reaped */
    int    fd;            /* the child's stdout */
    char   log[16384];
    size_t len;
} proc_t;

#define MAX_PROCS 4
static proc_t *started[MAX_PROCS];
static char statusd_path[PATH_MAX];

static void kill_all(void)
{
    for (int i = 0; i < MAX_PROCS; i++) {
        if (started[i] != NULL && started[i]->pid > 0) {
            kill(started[i]->pid, SIGKILL);
            waitpid(started[i]->pid, NULL, 0);
            started[i]->pid = 0;
        }
    }
}

static void sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* Reads whatever the child has printed, waiting up to ms for something. Returns the
 * bytes read. After the child closes its stdout poll() answers at once for ever, so
 * callers that wait must not count on pump() to take time. */
static size_t pump(proc_t *p, int ms)
{
    struct pollfd pf = { .fd = p->fd, .events = POLLIN };
    size_t before = p->len;

    if (p->fd < 0 || poll(&pf, 1, ms) <= 0) return 0;
    for (;;) {
        ssize_t r = read(p->fd, p->log + p->len, sizeof p->log - 1 - p->len);
        if (r <= 0) break;
        p->len += (size_t)r;
    }
    p->log[p->len] = '\0';
    return p->len - before;
}

/* Waits on the real clock, not on a count of loops. */
static bool wait_for(proc_t *p, const char *needle, int ms)
{
    uint64_t end = now_ms() + (uint64_t)ms;

    while (strstr(p->log, needle) == NULL && now_ms() < end)
        if (pump(p, 20) == 0) sleep_ms(5);
    return strstr(p->log, needle) != NULL;
}

static size_t occurrences(const char *hay, const char *needle)
{
    size_t n = 0;
    for (const char *s = hay; (s = strstr(s, needle)) != NULL; s += strlen(needle)) n++;
    return n;
}

/* Runs statusd with the given arguments (NULL-terminated). Stderr is dropped. */
static void start(proc_t *p, const char *const *args)
{
    char *argv[16] = { statusd_path };
    int fds[2];
    size_t n = 1;

    for (size_t i = 0; args[i] != NULL && n < 14; i++) argv[n++] = (char *)args[i];
    argv[n] = NULL;
    memset(p, 0, sizeof *p);
    if (pipe2(fds, O_CLOEXEC) < 0) { perror("pipe2"); exit(2); }

    p->pid = fork();
    if (p->pid < 0) { perror("fork"); exit(2); }
    if (p->pid == 0) {
        int null = open("/dev/null", O_WRONLY);
        prctl(PR_SET_PDEATHSIG, SIGKILL);              /* never outlive a test that crashes */
        dup2(fds[1], 1);
        if (null >= 0) dup2(null, 2);
        for (int fd = 3; fd < 256; fd++) close(fd);   /* nothing of the test's leaks in */
        execv(statusd_path, argv);
        _exit(127);
    }
    close(fds[1]);
    p->fd = fds[0];
    (void)fcntl(p->fd, F_SETFL, O_NONBLOCK);
    for (int i = 0; i < MAX_PROCS; i++)
        if (started[i] == NULL) { started[i] = p; break; }
}

/* The exit code (128 + n for death by signal n), or -1 if it is still running after ms. */
static int wait_exit(proc_t *p, int ms)
{
    uint64_t end = now_ms() + (uint64_t)ms;

    do {
        int st;
        pid_t r = waitpid(p->pid, &st, WNOHANG);
        if (r == p->pid) {
            p->pid = 0;
            pump(p, 0);
            return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
        }
        if (pump(p, 10) == 0) sleep_ms(5);
    } while (now_ms() < end);
    return -1;
}

/* Ends a process and forgets it: p is usually on the caller's stack, and must not be
 * looked at again by kill_all() after the caller returns. */
static void finish(proc_t *p)
{
    if (p->pid > 0) { kill(p->pid, SIGKILL); waitpid(p->pid, NULL, 0); p->pid = 0; }
    if (p->fd >= 0) { close(p->fd); p->fd = -1; }
    for (int i = 0; i < MAX_PROCS; i++)
        if (started[i] == p) started[i] = NULL;
}

/* Starts statusd on a free port and waits until it says it is listening. */
static uint16_t start_listening(proc_t *p, const char *const *extra)
{
    const char *args[10] = { "--bind", "127.0.0.1", "--port", "0" };
    size_t n = 4;

    for (size_t i = 0; extra != NULL && extra[i] != NULL && n < 8; i++) args[n++] = extra[i];
    args[n] = NULL;
    start(p, args);
    if (!wait_for(p, "listening on 127.0.0.1:", 5000)) {
        CHECK(false, "statusd never said it was listening");
        return 0;
    }
    const char *at = strstr(p->log, "listening on 127.0.0.1:") + strlen("listening on 127.0.0.1:");
    return (uint16_t)atoi(at);
}

/* ------------------------------------------------------------------- client */

static size_t get(uint16_t port, const char *request, char *out, size_t cap)
{
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };
    struct timeval tv = { 3, 0 };
    size_t len = 0;
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    out[0] = '\0';
    if (fd < 0) return 0;
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    if (connect(fd, (struct sockaddr *)&a, sizeof a) == 0 &&
        send(fd, request, strlen(request), MSG_NOSIGNAL) == (ssize_t)strlen(request)) {
        for (;;) {
            ssize_t r = recv(fd, out + len, cap - 1 - len, 0);
            if (r <= 0) break;
            len += (size_t)r;
        }
    }
    close(fd);
    out[len] = '\0';
    return len;
}

static int status_of(const char *r) { return strncmp(r, "HTTP/1.1 ", 9) == 0 ? atoi(r + 9) : -1; }

static char resp[65536];

static int fetch(uint16_t port, const char *path)
{
    char req[256];
    snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: test\r\n\r\n", path);
    get(port, req, resp, sizeof resp);
    return status_of(resp);
}

/* The newest "t_s" in the last response: the time of its latest sample. */
static unsigned long long latest_t_s(void)
{
    const char *last = NULL;
    for (const char *p = resp; (p = strstr(p, "\"t_s\":")) != NULL; p += 6) last = p + 6;
    return last != NULL ? strtoull(last, NULL, 10) : 0;
}

static bool can_bind(uint16_t port)
{
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };
    int one = 1;
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    bool ok;

    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    ok = bind(fd, (struct sockaddr *)&a, sizeof a) == 0;
    close(fd);
    return ok;
}

/* ------------------------------------------------------------------- tests */

static void test_usage_errors(void)
{
    static const char *const bad[][4] = {
        { "--port", "abc", NULL }, { "--port", "70000", NULL }, { "--port", "-1", NULL },
        { "--port", "", NULL },    { "--port", "80x", NULL },   { "--port", NULL },
        { "--bind", "999.1.1.1", NULL }, { "--bind", "localhost", NULL }, { "--bind", NULL },
        { "--heartbeat-s", "0", NULL }, { "--heartbeat-s", "x", NULL }, { "--heartbeat-s", "86401", NULL },
        { "--nope", NULL }, { "stray", NULL },
    };
    proc_t p;

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        start(&p, bad[i]);
        int code = wait_exit(&p, 3000);
        if (code != 2) fprintf(stderr, "  (%s %s: exit %d)\n", bad[i][0], bad[i][1] ? bad[i][1] : "", code);
        CHECK(code == 2, "a bad argument is a usage error, exit 2");
        finish(&p);
    }

    const char *help[] = { "--help", NULL };
    start(&p, help);
    CHECK(wait_exit(&p, 3000) == 0, "--help exits 0");
    CHECK(strstr(p.log, "usage") != NULL, "and prints the usage");
    finish(&p);
}

static void test_port_taken_is_exit_1(void)
{
    proc_t a, b;
    uint16_t port = start_listening(&a, NULL);
    char num[16];
    const char *args[] = { "--bind", "127.0.0.1", "--port", num, NULL };

    snprintf(num, sizeof num, "%u", (unsigned)port);
    start(&b, args);
    CHECK(wait_exit(&b, 3000) == 1, "a port in use is a runtime failure, exit 1");
    CHECK(strstr(b.log, "bind") != NULL, "and the log says it was the bind");
    finish(&b);
    finish(&a);
}

static void test_lifecycle(void)
{
    static const char *const fast[] = { "--heartbeat-s", "1", NULL };
    proc_t p;
    uint16_t port = start_listening(&p, fast);

    CHECK(port != 0, "a port was chosen");
    const char *line = strstr(p.log, "listening on");
    const char *eol = line != NULL ? strchr(line, '\n') : NULL;
    CHECK(line != NULL && memmem(p.log, (size_t)(line - p.log), "src=synth", 9) != NULL,
          "the log line is labeled src=synth");
    CHECK(eol != NULL, "a whole line");
    CHECK(wait_for(&p, "heartbeat rss_kib=", 2000), "a heartbeat right at the start");

    CHECK(fetch(port, "/api/status") == 200 && strstr(resp, "\"src\":\"synth\"") != NULL,
          "the API answers with synthetic records");
    CHECK(fetch(port, "/") == 200 && strstr(resp, "<title>carwatch</title>") != NULL, "and the page");
    get(port, "GARBAGE\r\n\r\n", resp, sizeof resp);
    CHECK(status_of(resp) == 400, "garbage is a 400");
    CHECK(fetch(port, "/api/faults") == 200, "and the server carries on");

    /* The synthetic source is driven by the real clock, and the clock moves. */
    CHECK(fetch(port, "/api/series?pid=0C") == 200, "a series");
    unsigned long long t1 = latest_t_s(), now = (unsigned long long)time(NULL);
    CHECK(t1 + 310 >= now && t1 <= now + 2, "its latest sample is from now");
    pump(&p, 0);
    sleep_ms(2500);
    CHECK(fetch(port, "/api/series?pid=0C") == 200 && latest_t_s() > t1,
          "and a later request has later samples");
    pump(&p, 100);
    CHECK(occurrences(p.log, "heartbeat rss_kib=") >= 3, "a heartbeat every second asked for");

    /* Descriptors: the listener, and nothing else beyond stdio, all close-on-exec. */
    char dir[64];
    snprintf(dir, sizeof dir, "/proc/%d/fd", (int)p.pid);
    DIR *d = opendir(dir);
    int open_fds = 0;
    struct dirent *e;
    while (d != NULL && (e = readdir(d)) != NULL) {
        int fd = atoi(e->d_name);
        if (e->d_name[0] == '.' || fd <= 2) continue;
        open_fds++;
        char info[64], buf[256];
        snprintf(info, sizeof info, "/proc/%d/fdinfo/%d", (int)p.pid, fd);
        FILE *f = fopen(info, "r");
        unsigned long flags = 0;
        while (f != NULL && fgets(buf, sizeof buf, f) != NULL)
            if (sscanf(buf, "flags: %lo", &flags) == 1) break;
        if (f != NULL) fclose(f);
        CHECK((flags & 02000000UL) != 0, "every descriptor is close-on-exec");
    }
    if (d != NULL) closedir(d);
    CHECK(d != NULL && open_fds == 1, "only the listening socket is open besides stdio");

    kill(p.pid, SIGTERM);
    CHECK(wait_exit(&p, 2000) == 0, "SIGTERM is a clean exit, 0");
    CHECK(strstr(p.log, "stop served=") != NULL, "and it logs the stop");
    CHECK(can_bind(port), "and the port is free at once");
    finish(&p);
}

/* A supervisor waits at most 60 s for a first sign of life (CLAUDE.md section 2.6), and the
 * default interval is an hour, so the first heartbeat must come at the start, not after it. */
static void test_heartbeat_at_start(void)
{
    proc_t p;

    start_listening(&p, NULL);
    CHECK(wait_for(&p, "heartbeat rss_kib=", 2000), "a heartbeat within 2 s of starting");
    pump(&p, 500);
    CHECK(occurrences(p.log, "heartbeat rss_kib=") == 1, "and just the one until the interval is up");
    finish(&p);
}

static void test_other_signals(void)
{
    proc_t p;
    uint16_t port = start_listening(&p, NULL);

    kill(p.pid, SIGINT);
    CHECK(wait_exit(&p, 2000) == 0, "SIGINT is a clean exit too");
    CHECK(can_bind(port), "port free after SIGINT");
    finish(&p);

    port = start_listening(&p, NULL);
    kill(p.pid, SIGKILL);
    CHECK(wait_exit(&p, 2000) == 128 + SIGKILL, "SIGKILL kills it");
    CHECK(can_bind(port), "and leaves nothing holding the port");
    finish(&p);
}

int main(void)
{
    ssize_t n = readlink("/proc/self/exe", statusd_path, sizeof statusd_path - 16);
    char *slash;

    if (n <= 0) { perror("readlink"); return 2; }
    statusd_path[n] = '\0';
    slash = strrchr(statusd_path, '/');
    if (slash == NULL) return 2;
    strcpy(slash + 1, "statusd");

    signal(SIGPIPE, SIG_IGN);
    atexit(kill_all);
    alarm(90);                       /* the whole test, however it hangs */

    test_usage_errors();
    test_port_taken_is_exit_1();
    test_lifecycle();
    test_heartbeat_at_start();
    test_other_signals();
    return failures == 0 ? 0 : 1;
}
