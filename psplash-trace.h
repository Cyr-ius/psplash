#ifndef _HAVE_PSPLASH_TRACE
#define _HAVE_PSPLASH_TRACE
#include <stdarg.h>

#define PATHFILE "/tmp"
#define TRACEFILE "/psplash-debug.log"

void psplash_trace(char *msg, ...);
#endif
