
bin_PROGRAMS = tcpkali

tcpkali_CPPFLAGS = -I$(top_srcdir)/deps/libev \
                   -I$(top_srcdir)/deps/libcows \
                   -I$(top_srcdir)/deps/libstatsd/src \
                   -I$(top_srcdir)/deps/HdrHistogram \
                   -I$(top_srcdir)/deps/boyer-moore-horspool
tcpkali_CFLAGS = -std=gnu99 $(TK_CFLAGS)
tcpkali_SOURCES = \
                tcpkali_dns.c tcpkali_dns.h               \
                tcpkali_engine.c tcpkali_engine.h         \
                tcpkali_syslimits.c tcpkali_syslimits.h   \
                tcpkali_signals.c tcpkali_signals.h       \
                tcpkali_pacefier.h tcpkali_atomic.h       \
                tcpkali_websocket.c tcpkali_websocket.h   \
                tcpkali_transport.c tcpkali_transport.h   \
                tcpkali_mavg.h tcpkali_events.h           \
                tcpkali_ring.c tcpkali_ring.h             \
                tcpkali_terminfo.c tcpkali_terminfo.h     \
                tcpkali_data.c tcpkali_data.h             \
                tcpkali_common.h tcpkali_rate.h           \
                tcpkali.c tcpkali.h
tcpkali_LDFLAGS = -L$(libdir) $(LIBUV)
tcpkali_LDADD = $(top_builddir)/deps/libev/libev.la \
                $(top_builddir)/deps/libcows/libcows.la \
                $(top_builddir)/deps/libstatsd/src/libstatsd.la \
                $(top_builddir)/deps/HdrHistogram/libhdr_histogram.la

check_tcpkali_ring_SOURCES = tcpkali_ring.c tcpkali_ring.h
check_tcpkali_ring_CFLAGS = -std=gnu99 $(TK_CFLAGS) -DTCPKALI_RING_UNIT_TEST

check_platform_SOURCES = check_platform.c
check_platform_CFLAGS = -std=gnu99 $(TK_CFLAGS)

TESTS = $(check_PROGRAMS)
check_PROGRAMS = check_tcpkali_ring check_platform
