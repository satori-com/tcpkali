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
#include <stdarg.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>

#include "tcpkali_syslimits.h"
#include "tcpkali_logging.h"

/*
 * Get a system setting, be it sysctl or a file contents, and put it into
 * the scanf variables.
 * RETURNS -1 if the system setting of a proper format was not found,
 *          0 otherwise.
 */
static int
vsystem_setting(const char *setting_name, const char *setting_fmt, va_list ap) {
    int n_args = 0;
    const char *p;

    /* Count the number of arguments to be extracted */
    for(p = setting_fmt; *p; p++) n_args += (*p == '%');

    /*
     * A file based setting starts with a slash: "/proc/cpu";
     * a sysctl(3) setting doesn't. Split by the slash first.
     */
    if(setting_name[0] == '/') {
        const char *filename = setting_name;
        FILE *f = fopen(filename, "r");
        if(!f) return -1;

        int scanned = vfscanf(f, setting_fmt, ap);
        fclose(f);
        return (scanned == n_args) ? 0 : -1;
    } else if(setting_fmt[0] == '\0'
              || (n_args == 1 && strcmp(setting_fmt, "%d") == 0)) {
#ifdef HAVE_SYSCTLBYNAME
        union {
            char buf[16];
            int integer;
        } contents;
        size_t contlen = sizeof(contents);
        if(sysctlbyname(setting_name, &contents, &contlen, NULL, 0) == -1
           || contlen == sizeof(contents))
            return -1;

        if(contlen != sizeof(contents.integer)) {
            warning("%s sysctl does not seem to hold an integer.\n",
                    setting_name);
            return -1;
        }

        if(n_args) {
            int *intp = va_arg(ap, int *);
            *intp = contents.integer;
        }

        return 0;
#else /* !HAVE_SYSCTLBYNAME */
        /* Explicitly convert sysctl-style into file-style for Linux */
        char filename[128] = "/proc/sys/";
        char *p = filename + strlen(filename);
#if !defined(__linux__)
#warning \
    "Converting sysctl-style parameters into file paths might not be compatible with non-Linux operating systems"
#endif
        for(; *setting_name && (size_t)(p - filename) < (sizeof(filename) - 1);
            setting_name++, p++) {
            switch(*setting_name) {
            case '.':
                *p = '/';
                break;
            default:
                *p = *setting_name;
            }
        }
        *p = '\0';
        return vsystem_setting(filename, setting_fmt, ap);
#endif /* HAVE_SYSCTLBYNAME */
    } else {
        assert(!"Unreachable");
        return -1;
    }
}

static int __attribute__((format(scanf, 2, 3)))
system_setting(const char *setting_name, const char *setting_fmt, ...) {
    va_list ap;
    va_start(ap, setting_fmt);
    int ret = vsystem_setting(setting_name, setting_fmt, ap);
    va_end(ap);
    return ret;
}

int
check_setsockopt_effect(int so_option) {
    const char *auto_rcvbuf[] = {"net.inet.tcp.doautorcvbuf", /* Mac OS X */
                                 "net.inet.tcp.recvbuf_auto", /* BSD */
                                 NULL};
    const char *auto_sndbuf[] = {"net.inet.tcp.doautorcvbuf", /* Mac OS X */
                                 "net.inet.tcp.sendbuf_auto", /* BSD */
                                 NULL};
    const char **auto_sysctls_to_check = 0;
    const char *so_name;

    switch(so_option) {
    case SO_RCVBUF:
        auto_sysctls_to_check = auto_rcvbuf;
        so_name = "SO_RCVBUF";
        break;
    case SO_SNDBUF:
        auto_sysctls_to_check = auto_sndbuf;
        so_name = "SO_SNDBUF";
        break;
    default:
        return -1;
    }

    if(auto_sysctls_to_check) {
        size_t i;
        for(i = 0; auto_sysctls_to_check[i]; i++) {
            int value;
            const char *sysctl_name = auto_sysctls_to_check[i];
            if(system_setting(sysctl_name, "%d", &value) == 0 && value == 1) {
                warning(
                    "The sysctl '%s=%d' prevents %s socket option "
                    "to make effect.\n",
                    sysctl_name, value, so_name);
                return 0;
            }
        }
    }

    /* Presume the socket option is effectful */
    return 1;
}


/*
 * Sort limits in descending order.
 */
static int
compare_rlimits(const void *ap, const void *bp) {
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
static rlim_t
max_open_files() {
    long value = sysconf(_SC_OPEN_MAX);
#ifdef OPEN_MAX
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
int
adjust_system_limits_for_highload(int expected_sockets, int workers) {
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
        expected_sockets * 2 + 100 + 2 * workers,
        expected_sockets + 100 + 2 * workers,
        expected_sockets + 4 + 2 * workers, /* n cores and other overhead */
    };
    size_t limits_count = sizeof(limits) / sizeof(limits[0]);
    int smallest_acceptable_fdmax = expected_sockets + 4 + 2 * workers;

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
                        (long)rlp.rlim_cur, (long)rlp.rlim_max,
                        strerror(errno));
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
               && limits[limits_count - 1] <= prev_limit.rlim_cur)) {
            return 0;
        }
        fprintf(stderr, "Could not adjust open files limit from %ld to %ld\n",
                (long)prev_limit.rlim_cur, (long)limits[limits_count - 1]);
        return -1;
    } else if(limits[i] < (rlim_t)smallest_acceptable_fdmax) {
        fprintf(stderr,
                "Adjusted open files limit from %ld to %ld, but still too low "
                "for --connections=%d.\n",
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
int
check_system_limits_sanity(int expected_sockets, int workers) {
    int return_value = 0;

    /*
     * Check that this process can open enough file descriptors.
     */
    int smallest_acceptable_fdmax = expected_sockets + 4 + 2 * workers;

    if(max_open_files() < (rlim_t)smallest_acceptable_fdmax) {
        const char *maxfiles_sctls[] = {"kern.maxfiles", "kern.maxfilesperproc",
                                        "fs.file-max"};
        size_t i;
        for(i = 0; i < sizeof(maxfiles_sctls) / sizeof(maxfiles_sctls[0]);
            i++) {
            const char *sysctl_name = maxfiles_sctls[i];
            int value;
            if(system_setting(sysctl_name, "%d", &value) != 0) continue;
            if(value < smallest_acceptable_fdmax) {
                warning(
                    "System-wide open files limit %d "
                    "is too low for the expected scale (-c %d).\n"
                    "Adjust the '%s' sysctl.\n",
                    value, expected_sockets, sysctl_name);
                return_value = -1;
            }
        }
        if(return_value != -1) {
            warning(
                "System-wide open files limit %d "
                "is too low for the expected scale (-c %d).\n",
                (int)max_open_files(), expected_sockets);
            return_value = -1;
        }
    }

    struct rlimit rlp;
    int ret;
    ret = getrlimit(RLIMIT_NOFILE, &rlp);
    assert(ret == 0);
    if(rlp.rlim_cur < (rlim_t)smallest_acceptable_fdmax) {
        warning(
            "Open files limit (`ulimit -n`) %ld "
            "is too low for the expected scale (-c %d).\n",
            (long)rlp.rlim_cur, expected_sockets);
        return_value = -1;
    }


    /*
     * Check that our system has enough ephemeral ports to open
     * expected_sockets to the destination.
     */
    const char *portrange_filename = "/proc/sys/net/ipv4/ip_local_port_range";
    const char *portrange_sysctl_lo = "net.inet.ip.portrange.first";
    const char *portrange_sysctl_hi = "net.inet.ip.portrange.last";
    int range_lo, range_hi;
    if(system_setting(portrange_filename, "%d %d", &range_lo, &range_hi) == 0) {
        if(range_hi - range_lo < expected_sockets) {
            warning(
                "Will not be able to open %d simultaneous connections "
                "since \"%s\" specifies too narrow range [%d..%d].\n",
                expected_sockets, portrange_filename, range_lo, range_hi);
            return_value = -1;
        }
    } else if(system_setting(portrange_sysctl_lo, "%d", &range_lo) == 0
              && system_setting(portrange_sysctl_hi, "%d", &range_hi) == 0) {
        /*
         * Check the ephemeral port range on BSD-derived systems.
         */
        if(range_hi - range_lo < expected_sockets) {
            warning(
                "Will not be able to open %d simultaneous connections "
                "since \"%s\" and \"%s\" sysctls specify too narrow "
                "range [%d..%d].\n",
                expected_sockets, portrange_sysctl_lo, portrange_sysctl_hi,
                range_lo, range_hi);
            return_value = -1;
        }
    }


    /*
     * Check that we are able to reuse the sockets when opening a lot
     * of connections over the short period of time.
     * http://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux.html
     */
    const char *time_wait_reuse_filename = "/proc/sys/net/ipv4/tcp_tw_reuse";
    int tcp_tw_reuse;
    if(system_setting(time_wait_reuse_filename, "%d", &tcp_tw_reuse) == 0) {
        if(tcp_tw_reuse != 1 && expected_sockets > 100) {
            warning(
                "Not reusing TIME_WAIT sockets, "
                "might not open %d simultaneous connections. "
                "Adjust \"%s\" value.\n",
                expected_sockets, time_wait_reuse_filename);
            return_value = -1;
        }
    }

    /*
     * Check that IP filter would allow tracking such many connections.
     * Also see net.netfilter.nf_conntrack_buckets, should be reasonable value,
     * such as nf_conntrack_max/4.
     * See http://serverfault.com/questions/482480/
     */
    const char *nf_conntrack_filename =
        "/proc/sys/net/netfilter/nf_conntrack_max";
    int nf_conntrack_max;
    if(system_setting(nf_conntrack_filename, "%d", &nf_conntrack_max) == 0) {
        if(expected_sockets > nf_conntrack_max) {
            warning(
                "IP filter might not allow "
                "opening %d simultaneous connections. "
                "Adjust \"%s\" value.\n",
                expected_sockets, nf_conntrack_filename);
            return_value = -1;
        }
    }

    /*
     * Check that IP filter tracking is efficient by checking buckets.
     * If number of buckets is severely off the optimal value, lots of
     * CPU will be wasted.
     * See http://serverfault.com/questions/482480/
     */
    const char *nf_hash_filename =
        "/sys/module/nf_conntrack/parameters/hashsize";
    int nf_hashsize;
    if(system_setting(nf_hash_filename, "%d", &nf_hashsize) == 0) {
        if(nf_hashsize < (expected_sockets / 8)) {
            warning(
                "IP filter is not properly sized for "
                "tracking %d simultaneous connections. "
                "Adjust \"%s\" value to at least %d.\n",
                expected_sockets, nf_hash_filename, expected_sockets / 8);
            return_value = -1;
        }
    }

    return return_value;
}
