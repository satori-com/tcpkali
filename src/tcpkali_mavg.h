/*
 * Copyright (c) 2014, 2017  Machine Zone, Inc.
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
    const double decay_factor;
    double accumulator;
} exp_moving_average;

static void __attribute__((unused))
exp_moving_average_init(exp_moving_average *ema, double decay) {
    assert(decay > 0 && decay <= 1);
    *(double *)&ema->decay_factor = decay;
    ema->accumulator = 0 / 0.0;
}

static void __attribute__((unused))
exp_moving_average_add(exp_moving_average *ema, double x) {
    if(isfinite(ema->accumulator) == 0)
        ema->accumulator = x;
    else
        ema->accumulator =
            ema->decay_factor * x + (1 - ema->decay_factor) * ema->accumulator;
}

typedef struct {
    const double aggregate_window; /* Time window to aggregate data in. */
    double events;              /* Aggregated events for rate calculation. */
    double right_edge_ts;       /* Time to aggregate a new rate point. */
    double update_ts;           /* Last time we got an update. */
    exp_moving_average storage; /* Aggregated measurement results. */
} mavg;

/*
 * Initialize the empty moving average structure. Keep the smoothing window
 * within the structure.
 * Typical size of the aggregate_window is (1/8.0) seconds.
 * Typical size of the smoothing window is 3.0 seconds.
 */
static void __attribute__((unused))
mavg_init(mavg *m, double start_time, double aggregate_window,
                  double smoothing_window) {
    assert(aggregate_window <= smoothing_window);
    exp_moving_average_init(&m->storage,
                            aggregate_window / smoothing_window);
    m->events = 0;
    *(double *)&m->aggregate_window = aggregate_window;
    m->right_edge_ts = start_time + aggregate_window;
    m->update_ts = start_time;
}

static double __attribute__((unused))
mavg_smoothing_window_s(const mavg *m) {
    return m->aggregate_window / m->storage.decay_factor;
}

/*
 * Bump the moving average with the specified number of events.
 */
static void __attribute__((unused))
mavg_add(mavg *m, double now, double new_events) {

    if(new_events == 0.0 && m->events == 0.0)
        return;

    assert(!(new_events < 0));

    if(m->right_edge_ts > now) {
        /*
         * We are still aggregating results and has not reached
         * the edge of the update window
         */
        m->events += new_events;
        m->update_ts = now;
        return;
    }

    if(m->right_edge_ts + mavg_smoothing_window_s(m) < now) {
        m->right_edge_ts = now + m->aggregate_window;
        m->update_ts = now;
        m->events = 0;
        m->storage.accumulator = new_events / m->aggregate_window;
        return;
    }

    /*
     * Using a model where events arrive at the same rate,
     * distribute them between timeframe to measure/store,
     * and future events.
     */
    double old_events = new_events * (m->right_edge_ts - m->update_ts)
                        / (now - m->update_ts);
    exp_moving_average_add(&m->storage,
                          (m->events + old_events) / m->aggregate_window);
    m->update_ts = m->right_edge_ts;
    m->right_edge_ts += m->aggregate_window;
    m->events = 0;

    /* If last event was long ago - we want to make a set of updates */
    mavg_add(m, now, new_events - old_events);
}

static double __attribute__((unused)) mavg_per_second(const mavg *m, double now) {
    double elapsed = now - m->update_ts;
    if(elapsed > mavg_smoothing_window_s(m)) {
        /*
         * If we stopped for too long, we report zero.
         * Otherwise we'll never reach zero (traffic, hits/s),
         * just asymptotically approach it.
         * This is confusing for humans. So we report zero.
         */
        return 0.0;
    } else if(isfinite(m->storage.accumulator)) {
        return m->storage.accumulator
               * pow(1 - m->storage.decay_factor,
                     (now - m->update_ts) / m->aggregate_window);
    } else {
        return 0.0;
    }
}


#endif /* TCPKALI_MAVG_H */
