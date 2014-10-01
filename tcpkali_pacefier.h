/*
    tcpkali: fast multi-core TCP load generator.

    Original author: Lev Walkin <lwalkin@machinezone.com>

    Copyright (C) 2014  Machine Zone, Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
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
static inline size_t
pacefier_allow(struct pacefier *p, double events_per_second, double now) {
    double elapsed = now - p->previous_ts;
    ssize_t emit_events = elapsed * events_per_second;  /* Implicit rounding */
    if(emit_events > 0)
        return emit_events;
    else
        return 0;
}

/*
 * Record the actually emitted events.
 */
static inline void
pacefier_emitted(struct pacefier *p, double events_per_second, size_t emitted, double now) {
    double elapsed = now - p->previous_ts;
    double emit_events = elapsed * events_per_second;
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
    /*
     * If the process cannot keep up with the pace, it will result in
     * previous_ts shifting in the past more and more with time.
     * We don't allow more than 5 seconds skew to prevent too sudden
     * bursts of events and to avoid overfilling the integers.
     */
    if((now - p->previous_ts) > 5)
        p->previous_ts = now - 5;
}


#endif  /* TCPKALI_PACEFIER_H */
