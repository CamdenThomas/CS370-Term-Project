/* Boundary self-test: even a Unix socket must go through src/ipc/uds.c. */
#include <sys/socket.h>
int f(void) { return socket(AF_UNIX, SOCK_SEQPACKET, 0); }
