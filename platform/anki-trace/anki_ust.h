#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER anki_ust

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "platform/anki-trace/anki_ust.h"

#if !defined(ANKI_UST_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define ANKI_UST_H

#if defined(USE_ANKITRACE)
#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(
    anki_ust,
    anki_fault_code,
    TP_ARGS(int , code),
    TP_FIELDS(ctf_integer(int, c, code)
    )
)

TRACEPOINT_EVENT(
    anki_ust,
    vic_anim_loop_duration,
    TP_ARGS(long long, duration),
    TP_FIELDS(ctf_integer(long long, duration, duration))
)

TRACEPOINT_EVENT(
    anki_ust,
    vic_robot_loop_duration,
    TP_ARGS(long long, duration),
    TP_FIELDS(ctf_integer(long long, duration, duration))
)

TRACEPOINT_EVENT(
    anki_ust,
    vic_robot_robot_loop_period,
    TP_ARGS(long long, delay),
    TP_FIELDS(ctf_integer(long long, delay, delay))
)

TRACEPOINT_EVENT(
    anki_ust,
    vic_engine_loop_duration,
    TP_ARGS(long long, duration),
    TP_FIELDS(ctf_integer(long long, duration, duration))
)

#endif /* ANKITRACE */
#endif /* ANKI_UST_H */

#include <lttng/tracepoint-event.h>

