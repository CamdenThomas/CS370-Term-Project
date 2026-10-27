/* Boundary self-test: the page server may serve the box's own access point. */
#include <netinet/in.h>
#include <sys/socket.h>
int page_listen(void) { return socket(AF_INET, SOCK_STREAM, 0); }
