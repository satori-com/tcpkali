
bin_PROGRAMS = tcpkali

AM_YFLAGS = -d
AM_LFLAGS = -s

tcpkali_CPPFLAGS = $(TK_CPPFLAGS) \
                   -I$(top_srcdir)/asn1 \
                   -I$(top_srcdir)/deps/libev \
                   -I$(top_srcdir)/deps/libcows \
                   -I$(top_srcdir)/deps/libstatsd/src \
                   -I$(top_srcdir)/deps/HdrHistogram \
                   -I$(top_srcdir)/deps/boyer-moore-horspool \
                   -I$(top_srcdir)/deps/pcg-c-basic
tcpkali_CFLAGS = -std=gnu99 $(TK_CFLAGS)
tcpkali_SOURCES = \
                tcpkali_iface.c tcpkali_iface.h           \
                tcpkali_dns.c tcpkali_dns.h               \
                tcpkali_engine.c tcpkali_engine.h         \
                tcpkali_syslimits.c tcpkali_syslimits.h   \
                tcpkali_signals.c tcpkali_signals.h       \
                tcpkali_pacefier.h tcpkali_atomic.h       \
                tcpkali_websocket.c tcpkali_websocket.h   \
                tcpkali_transport.c tcpkali_transport.h   \
                tcpkali_regex.c tcpkali_regex.h           \
                tcpkali_mavg.h tcpkali_events.h           \
                tcpkali_ring.c tcpkali_ring.h             \
                tcpkali_terminfo.c tcpkali_terminfo.h     \
                tcpkali_data.c tcpkali_data.h             \
                tcpkali_expr_y.c  tcpkali_expr_y.h        \
                                 tcpkali_expr_l.c         \
                tcpkali_expr.c tcpkali_expr.h             \
                tcpkali_logging.c tcpkali_logging.h       \
                tcpkali_traffic_stats.h                   \
                tcpkali_common.h tcpkali_rate.h           \
                tcpkali_statsd.c tcpkali_statsd.h         \
                tcpkali_run.c tcpkali_run.h               \
                tcpkali_ssl.c tcpkali_ssl.h               \
                tcpkali_connection.c tcpkali_connection.h \
                tcpkali.c tcpkali.h
tcpkali_LDFLAGS = $(TK_LDFLAGS) $(TK_LIBS) -L$(libdir) $(LIBUV)
tcpkali_LDADD = $(top_builddir)/asn1/libasncodec.la \
                $(top_builddir)/deps/libev/libev.la \
                $(top_builddir)/deps/libcows/libcows.la \
                $(top_builddir)/deps/pcg-c-basic/libpcg32.la \
                $(top_builddir)/deps/libstatsd/src/libstatsd.la \
                $(top_builddir)/deps/HdrHistogram/libhdr_histogram.la

check_platform_SOURCES = check_platform.c
check_platform_CFLAGS = -std=gnu99 $(TK_CFLAGS) -I$(top_srcdir)/asn1

check_tcpkali_ring_SOURCES = tcpkali_ring.c tcpkali_ring.h
check_tcpkali_ring_CFLAGS = -std=gnu99 $(TK_CFLAGS) -DTCPKALI_RING_UNIT_TEST

check_tcpkali_regex_SOURCES = tcpkali_regex.c tcpkali_regex.h $(top_srcdir)/deps/pcg-c-basic/pcg_basic.c
check_tcpkali_regex_CFLAGS = -std=gnu99 $(TK_CFLAGS) -I$(top_srcdir)/deps/pcg-c-basic -DTCPKALI_REGEX_UNIT_TEST

check_tcpkali_iface_SOURCES = tcpkali_iface.c tcpkali_iface.h tcpkali_logging.c tcpkali_logging.h tcpkali_terminfo.c tcpkali_terminfo.h
check_tcpkali_iface_CFLAGS = -std=gnu99 $(TK_CFLAGS) -DTCPKALI_IFACE_UNIT_TEST -I$(top_srcdir)/asn1

TESTS = $(check_PROGRAMS) ${dist_check_SCRIPTS}
check_PROGRAMS = check_platform check_tcpkali_ring check_tcpkali_regex check_tcpkali_iface 

dist_check_SCRIPTS = # check_code_format.sh

reformat:
	${CLANG_FORMAT} -i $(shell ls *.[ch] | grep -v "^tcpkali_expr_[yl].[ch]")
