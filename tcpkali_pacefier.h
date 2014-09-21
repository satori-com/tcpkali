/*
 * Pacefyer is a structure to limit (pace) the emission of events
 * in the system by controlling the rate and number of events produced.
 */
#ifndef TCPKALI_PACEFIER_H
#define TCPKALI_PACEFIER_H

struct pacefier {
    double previous_ts;
};

static inline void
pacefier_init(struct pacefier *p, double now) {
    p->previous_ts = now;
}

/*
 * Get the number of events we can emit now, since we've advanced our time
 * forward a little.
 */
static inline int
pacefier_allow(struct pacefier *p, double events_per_second, double now) {
    double elapsed = now - p->previous_ts;
    int emit_events = elapsed * events_per_second;  /* Implicit rounding */
    return emit_events;
}

/*
 * Record the actually emitted events.
 */
static inline void
pacefier_emitted(struct pacefier *p, double events_per_second, int emitted, double now) {
    double elapsed = now - p->previous_ts;
    double emit_events = elapsed * events_per_second;
    assert(emit_events >= emitted);

    /*
     * The number of allowed events is almost always less
     * than what's actually computed, due to rounding.
     * That means that we can't just say that the nearest event
     * should be emitted at exactly (now + 1.0/events_per_second) seconds.
     * We'd need to accomodate the unsent fraction of events that
     * should have been but weren't emitted in the previous step.
     * We do that by not setting previous_ts to now, but by pushing
     * it a little to the past to allow the next pacefier_allow() operation
     * to produce a tiny little bit greater number.
     * Test: if at t1 we were allowed to emit EPS=5 events and emitted 4,
     * that means that at we should record t1-(5-4/5) instead of t1.
     */
    
    p->previous_ts = now - (emit_events - emitted)/events_per_second;
}


#endif  /* TCPKALI_PACEFIER_H */
