
bin_PROGRAMS = tcpkali

tcpkali_CPPFLAGS = -I$(top_srcdir)/deps/libev \
                   -I$(top_srcdir)/deps/libcows \
                   -I$(top_srcdir)/deps/libstatsd/src
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
                tcpkali.c tcpkali.h
tcpkali_LDFLAGS = -L$(libdir) $(LIBUV)
tcpkali_LDADD = $(top_builddir)/deps/libev/libev.la \
                $(top_builddir)/deps/libcows/libcows.la \
                $(top_builddir)/deps/libstatsd/src/libstatsd.la

