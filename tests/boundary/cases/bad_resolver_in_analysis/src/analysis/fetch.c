/* Boundary self-test: name resolution outside src/interface/ must be rejected. */
#include <stddef.h>
struct addrinfo;
int getaddrinfo(const char *, const char *, const void *, struct addrinfo **);
int f(void) { return getaddrinfo("example.org", "80", NULL, NULL); }
