#ifndef TRACING_H
#define TRACING_H
#if defined(USE_ANKITRACE)
#include <lttng/tracelog.h>
#include "platform/anki-trace/anki_ust.h"
#define ANKITRACE_ENABLED 1
#else
#define tracepoint(...)
#define tracelog(e,m,...)
#define ANKITRACE_ENABLED 0
#endif
#endif
