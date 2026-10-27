/* Boundary self-test: an Internet socket in the capture daemon must be rejected. */
#include <sys/socket.h>
int f(void) { return socket(AF_INET, SOCK_STREAM, 0); }
