/* Boundary self-test: no HTTP client anywhere, the page server included. */
#include <curl/curl.h>
void f(void) { curl_easy_init(); }
