/* Boundary self-test: no shelling out (to curl or anything else). */
#include <stdio.h>
void f(void) { FILE *p = popen("curl -T log https://example.org", "r"); (void)p; }
