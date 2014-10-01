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
    if(value != -1) {
        return value;
    } else {
        perror("sysconf(_SC_OPEN_MAX)");
#ifdef  OPEN_MAX
        return OPEN_MAX;
#else
        return 1024;
#endif
    }
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
        prev_limit.rlim_max != RLIM_INFINITY ? prev_limit.rlim_max : max_open,
        expected_sockets + 100 + workers,
        expected_sockets + 4 + workers, /* n cores and other overhead */
    };
    size_t limits_count = sizeof(limits)/sizeof(limits[0]);

    qsort(limits, limits_count, sizeof(limits[0]), compare_rlimits);

    /* Current limits exceed requirements, OK. */
    if(prev_limit.rlim_cur >= limits[0]) {
        return 0;
    }

    /*
     * Attempt to set the largest limit out of the given set.
     */
    size_t i;
    for(i = 0; i < limits_count; i++) {
        struct rlimit rlp;
        rlp.rlim_cur = limits[i];
        rlp.rlim_max = limits[i];
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
        fprintf(stderr, "Could not adjust open files limit from %ld to %ld\n",
            (long)prev_limit.rlim_cur, (long)limits[limits_count - 1]);
        return -1;
    } else if(limits[i] < (size_t)(expected_sockets + 4 + workers)) {
        fprintf(stderr, "Adjusted limit from %ld to %ld, but still too low for --connections=%d.\n",
            (long)prev_limit.rlim_cur, (long)limits[i], expected_sockets);
        return -1;
    } else {
        fprintf(stderr, "Adjusted open files limit from %ld to %ld.\n",
            (long)prev_limit.rlim_cur, (long)limits[i]);
        return 0;
    }
}


int check_system_limits_sanity(int expected_sockets, int workers) {

    /*
     * Check that this process can open enough file descriptors.
     */
    struct rlimit rlp;
    int ret;
    ret = getrlimit(RLIMIT_NOFILE, &rlp);
    assert(ret == 0);

    if(rlp.rlim_cur < (rlim_t)(expected_sockets + 4 + workers)) {
        fprintf(stderr, "Warning: Open files limit (`ulimit -n`) %ld "
                        "is too low for the expected load (-c %d).\n",
            (long)rlp.rlim_cur, expected_sockets);
        return -1;
    }

    if(max_open_files() < (rlim_t)(expected_sockets + 4 + workers)) {
        fprintf(stderr, "Warning: System files limit (`ulimit -n`) %ld "
                        "is too low for the expected load (-c %d).\n",
            (long)rlp.rlim_cur, expected_sockets);
        return -1;
    }


    /*
     * Check that our system has enough ephemeral ports to open
     * expected_sockets to the destination.
     */
    const char *portrange_filename = "/proc/sys/net/ipv4/ip_local_port_range";
    FILE *f = fopen("/proc/sys/net/ipv4/ip_local_port_range", "r");
    if(f) {
        int lo, hi;
        if(fscanf(f, "%d %d", &lo, &hi) == 2) {
            if(hi - lo < expected_sockets) {
                fprintf(stderr, "Will not be able to open "
                    "%d simultaneous connections "
                    "since %s specifies too small range [%d..%d]\n",
                    expected_sockets, portrange_filename, lo, hi);
            }
        }
        fclose(f);
    }

    return 0;
}

