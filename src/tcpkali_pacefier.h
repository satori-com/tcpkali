/*
 * Copyright (c) 2014, 2015  Machine Zone, Inc.
 *
 * Original author: Lev Walkin <lwalkin@machinezone.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef TCPKALI_PACEFIER_H
#define TCPKALI_PACEFIER_H

struct pacefier {
    double previous_ts;
    double events_per_second;
};

static inline void
pacefier_init(struct pacefier *p, double events_per_second, double now) {
    p->previous_ts = now;
    p->events_per_second = events_per_second;
}

/*
 * Get the number of events we can move now, since we've advanced our time
 * forward a little.
 */
static inline size_t
pacefier_allow(struct pacefier *p, double now) {
    double elapsed = now - p->previous_ts;
    ssize_t move_events = elapsed * p->events_per_second; /* Implicit rounding */
    if(move_events > 0)
        return move_events;
    else
        return 0;
}

/*
 * Get the delay until the time we're allowed to move (need_events).
 */
static inline double
pacefier_when_allowed(struct pacefier *p, double now,
                      size_t need_events) {
    double elapsed = now - p->previous_ts;
    double move_events = elapsed * p->events_per_second;
    if(move_events >= need_events) {
        return 0.0;
    } else {
        return (need_events - move_events) / p->events_per_second;
    }
}

/*
 * Record the actually moved events.
 */
static inline void
pacefier_moved(struct pacefier *p, size_t moved, double now) {
    /*
     * The number of allowed events is almost always less
     * than what's actually computed.
     */
    p->previous_ts += moved / p->events_per_second;
    /*
     * If the process cannot keep up with the pace, it will result in
     * previous_ts shifting in the past more and more with time.
     * We don't allow more than 2/Rate seconds skew to prevent too sudden
     * bursts of events and to avoid overfilling the integers.
     */
    double wiggle_seconds;
    if(p->events_per_second > 1.0) {
        wiggle_seconds = 2.0;
    } else {
        wiggle_seconds = 2.0 / p->events_per_second;
    }
    if((now - p->previous_ts) > wiggle_seconds) {
        p->previous_ts = now - wiggle_seconds;
    }
}


#endif /* TCPKALI_PACEFIER_H */
