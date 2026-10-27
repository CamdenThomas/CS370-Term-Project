/* Boundary self-test: the one place a Unix socket is opened. */
#include <sys/socket.h>
int uds_open(void) { return socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0); }
