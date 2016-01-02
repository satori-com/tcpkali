/*
 * Copyright (c) 2014, 2015, 2016  Machine Zone, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <assert.h>

#include "tcpkali_syslimits.h"
#include "tcpkali_logging.h"

/*
 * Sort limits in descending order.
 */
static int compare_rlimits(const void *ap, const void *bp) {
    rlim_t a = *(rlim_t *)ap;
    rlim_t b = *(rlim_t *)bp;
    if(a < b)
        return 1;
    else if(a > b)
        return -1;
    return 0;
}

/*
 * Determine the global limit on open files.
 */
static rlim_t max_open_files() {
    long value = sysconf(_SC_OPEN_MAX);
#ifdef  OPEN_MAX
    if(value != -1) {
        return value > OPEN_MAX ? value : OPEN_MAX;
    } else {
        perror("sysconf(_SC_OPEN_MAX)");
        return OPEN_MAX;
    }
#else
    if(value != -1) {
        return value;
    } else {
        perror("sysconf(_SC_OPEN_MAX)");
        return 1024;
    }
#endif
}

/*
 * Adjust number of open files.
 */
int adjust_system_limits_for_highload(int expected_sockets, int workers) {
    rlim_t max_open = max_open_files();
    struct rlimit prev_limit;
    int ret;

    ret = getrlimit(RLIMIT_NOFILE, &prev_limit);
    assert(ret == 0);

    /*
     * The engine consumes file descriptors for its internal needs,
     * and each one of the expected_sockets is a file descriptor, naturally.
     * So we account for some overhead and attempt to set the largest possible
     * limit. Also, the limit cannot be defined precisely, since there can
     * be arbitrary spikes. So we want to set our limit as high as we can.
     */
    rlim_t limits[] = {
        max_open,
        prev_limit.rlim_max != RLIM_INFINITY ? prev_limit.rlim_max : max_open,
        expected_sockets * 2 + 100 + workers,
        expected_sockets + 100 + workers,
        expected_sockets + 4 + workers, /* n cores and other overhead */
    };
    size_t limits_count = sizeof(limits)/sizeof(limits[0]);

    qsort(limits, limits_count, sizeof(limits[0]), compare_rlimits);

    /*
     * Attempt to set the largest limit out of the given set.
     */
    size_t i;
    for(i = 0; i < limits_count; i++) {
        struct rlimit rlp;
        rlp.rlim_cur = limits[i];
        rlp.rlim_max = RLIM_INFINITY;
        if(setrlimit(RLIMIT_NOFILE, &rlp) == -1) {
            if(errno == EPERM || errno == EINVAL) {
                continue;
            } else {
                fprintf(stderr, "setrlimit(RLIMIT_NOFILE, {%ld, %ld}): %s\n",
                    (long)rlp.rlim_cur, (long)rlp.rlim_max, strerror(errno));
                return -1;
            }
        } else {
            break;
        }
    }

    if(i == limits_count) {
        /*
         * Some continuous integration environments do not allow messing
         * with rlimits.
         * If we have just enough file descriptors to satisfy our test case,
         * ignore failures to adjust rlimits.
         */
        if(expected_sockets == 0
        || (expected_sockets > 0
                && limits[limits_count-1] <= prev_limit.rlim_cur)) {
            return 0;
        }
        fprintf(stderr, "Could not adjust open files limit from %ld to %ld\n",
            (long)prev_limit.rlim_cur, (long)limits[limits_count - 1]);
        return -1;
    } else if(limits[i] < (rlim_t)(expected_sockets + 4 + workers)) {
        fprintf(stderr, "Adjusted open files limit from %ld to %ld, but still too low for --connections=%d.\n",
            (long)prev_limit.rlim_cur, (long)limits[i], expected_sockets);
        return -1;
    } else if(expected_sockets == 0) {
        fprintf(stderr, "Adjusted open files limit from %ld to %ld.\n",
            (long)prev_limit.rlim_cur, (long)limits[i]);
        return 0;
    } else {
        return 0;
    }
}

/*
 * Check that the limits are sane and print out if not.
 */
int check_system_limits_sanity(int expected_sockets, int workers) {
    int return_value = 0;

    /*
     * Check that this process can open enough file descriptors.
     */
    struct rlimit rlp;
    int ret;
    ret = getrlimit(RLIMIT_NOFILE, &rlp);
    assert(ret == 0);

    if(rlp.rlim_cur < (rlim_t)(expected_sockets + 4 + workers)) {
        warning("Open files limit (`ulimit -n`) %ld "
                "is too low for the expected scale (-c %d).\n",
                (long)rlp.rlim_cur, expected_sockets);
        return_value = -1;
    } else if(max_open_files() < (rlim_t)(expected_sockets + 4 + workers)) {
        warning("System-wide open files limit %ld "
                "is too low for the expected scale (-c %d).\n"
                "Consider adjusting fs.file-max or kern.maxfiles sysctl.\n",
                (long)rlp.rlim_cur, expected_sockets);
        return_value = -1;
    }


    /*
     * Check that our system has enough ephemeral ports to open
     * expected_sockets to the destination.
     */
    const char *portrange_filename = "/proc/sys/net/ipv4/ip_local_port_range";
    FILE *f = fopen(portrange_filename, "r");
    if(f) {
        int lo, hi;
        if(fscanf(f, "%d %d", &lo, &hi) == 2) {
            if(hi - lo < expected_sockets) {
                warning("Will not be able to open %d simultaneous connections "
                        "since \"%s\" specifies too narrow range [%d..%d].\n",
                        expected_sockets, portrange_filename, lo, hi);
                return_value = -1;
            }
        }
        fclose(f);
    }

    /*
     * Check that we are able to reuse the sockets when opening a lot
     * of connections over the short period of time.
     * http://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux.html
     */
    const char *time_wait_reuse_filename = "/proc/sys/net/ipv4/tcp_tw_reuse";
    f = fopen(time_wait_reuse_filename, "r");
    if(f) {
        int flag;
        if(fscanf(f, "%d", &flag) == 1) {
            if(flag != 1 && expected_sockets > 100) {
                warning("Not reusing TIME_WAIT sockets, "
                        "might not open %d simultaneous connections. "
                        "Adjust \"%s\" value.\n",
                        expected_sockets, time_wait_reuse_filename);
                return_value = -1;
            }
        }
        fclose(f);
    }

    /*
     * Check that IP filter would allow tracking such many connections.
     * Also see net.netfilter.nf_conntrack_buckets, should be reasonable value,
     * such as nf_conntrack_max/4.
     * See http://serverfault.com/questions/482480/
     */
    const char *nf_conntrack_filename = "/proc/sys/net/netfilter/nf_conntrack_max";
    f = fopen(nf_conntrack_filename, "r");
    if(f) {
        int n;
        if(fscanf(f, "%d", &n) == 1) {
            if(expected_sockets > n) {
                warning("IP filter might not allow "
                        "opening %d simultaneous connections. "
                        "Adjust \"%s\" value.\n",
                        expected_sockets, nf_conntrack_filename);
                return_value = -1;
            }
        }
        fclose(f);
    }

    /*
     * Check that IP filter tracking is efficient by checking buckets.
     * If number of buckets is severely off the optimal value, lots of
     * CPU will be wasted.
     * See http://serverfault.com/questions/482480/
     */
    const char *nf_hash_filename = "/sys/module/nf_conntrack/parameters/hashsize";
    f = fopen(nf_hash_filename, "r");
    if(f) {
        int n;
        if(fscanf(f, "%d", &n) == 1) {
            if(n < (expected_sockets/8)) {
                warning("IP filter is not properly sized for "
                        "tracking %d simultaneous connections. "
                        "Adjust \"%s\" value to at least %d.\n",
                        expected_sockets, nf_hash_filename, expected_sockets/8);
                return_value = -1;
            }
        }
        fclose(f);
    }

    return return_value;
}

