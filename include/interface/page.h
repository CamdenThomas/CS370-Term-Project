/* The status page (D-013, D-045): one HTML document, with its style and script inline,
 * embedded in the binary. The device is offline, so it names no other host and loads
 * nothing from one. route.c serves it at "/". */
#ifndef CARWATCH_PAGE_H
#define CARWATCH_PAGE_H

extern const char page_html[];

#endif
