/*
 * Copyright (c) 2014  Machine Zone, Inc.
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
/*
 * Moving average allows to capture number of events per period of time
 * (hits/second, kbps)
 * averaged over the specified time period.
 * http://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average
 */
#ifndef TCPKALI_MAVG_H
#define TCPKALI_MAVG_H

#include <string.h>
#include <math.h>
#include <assert.h>

typedef struct {
    double
        unprocessed_events; /* Events not incorporated into historic_average. */
    double smoothing_window; /* Smooth (produce moving average) over the given
                                time. */
    double last_update_ts;   /* Last time we updated the moving average. */
    double historic_average; /* Number of events in the smoothing window. */
} mavg;

/*
 * Figure out how often we aggregate data.
 * Aggregate the events at least 8 times per smoothing window
 * (if the window is tiny), but no less frequently than 8 times a second.
 */
static double __attribute__((unused)) mavg_aggregate_over(mavg *m) {
    return m->smoothing_window > 1.0 ? 0.125 : m->smoothing_window / 8;
}

/*
 * Initialize the empty moving average structure. Keep the smoothing window
 * within the structure.
 */
static void __attribute__((unused))
mavg_init(mavg *m, double now, double window) {
    assert(window > 0.0);
    memset(m, 0, sizeof(*m));
    m->smoothing_window = window;
    m->last_update_ts = now;
    m->historic_average = 0 / 0.0;
}

/*
 * Bump the moving average with the specified number of events.
 */
static void __attribute__((unused))
mavg_bump(mavg *m, double now, double events) {
    double aggregate_over = mavg_aggregate_over(m);
    double elapsed = now - m->last_update_ts;

    if(events == 0.0 && m->unprocessed_events == 0.0) return;

    if(elapsed < aggregate_over) {
        m->unprocessed_events += events;
    } else {
        /*
         * Start fast: do not slowly ramp up over smoothing_window.
         * Just extrapolate with what we know already.
         */
        if(isfinite(m->historic_average) == 0) {
            m->historic_average =
                aggregate_over * (m->unprocessed_events + events) / elapsed;
            m->unprocessed_events = 0;
            m->last_update_ts = now;
            return;
        }

        double window = m->smoothing_window;
        double unproc = m->unprocessed_events;
        double prev_win_avg =
            unproc
            + (m->historic_average - unproc) * exp(-aggregate_over / window);
        m->historic_average =
            prev_win_avg * exp((aggregate_over - elapsed) / window);
        m->unprocessed_events = events; /* Save these new events for later. */
        m->last_update_ts = now;
    }
}

static double __attribute__((unused)) mavg_per_second(mavg *m, double now) {
    double elapsed = now - m->last_update_ts;
    if(elapsed > m->smoothing_window) {
        /*
         * If we stopped for too long, we report zero.
         * Otherwise we'll never reach zero (traffic, hits/s),
         * just asymptotically approach it.
         * This is confusing for humans. So we report zero.
         */
        return 0.0;
    } else if(isfinite(m->historic_average)) {
        if(m->unprocessed_events) {
            mavg_bump(m, now, 0);
        }
        double aggregate_over = mavg_aggregate_over(m);
        double window = m->smoothing_window;
        double unproc = m->unprocessed_events;
        double prev_win_avg =
            unproc
            + (m->historic_average - unproc) * exp(-aggregate_over / window);
        double avg = prev_win_avg * exp((aggregate_over - elapsed) / window);
        return avg / aggregate_over;
    } else {
        return 0.0;
    }
}


#endif /* TCPKALI_MAVG_H */
