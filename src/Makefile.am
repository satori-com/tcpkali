
SUBDIRS = libev libstatsd

bin_PROGRAMS = tcpkali

tcpkali_CPPFLAGS = -I$(srcdir)/libev -I$(srcdir)/libstatsd/src
tcpkali_CFLAGS = -std=gnu99 $(TK_CFLAGS)
tcpkali_SOURCES = \
                tcpkali_dns.c tcpkali_dns.h               \
                tcpkali_engine.c tcpkali_engine.h         \
                tcpkali_syslimits.c tcpkali_syslimits.h   \
                tcpkali_signals.c tcpkali_signals.h       \
                tcpkali_pacefier.h tcpkali_atomic.h       \
                tcpkali_websocket.c tcpkali_websocket.h   \
                tcpkali_transport.c tcpkali_transport.h   \
                tcpkali_mavg.h                            \
                tcpkali.c tcpkali.h
tcpkali_LDADD = $(builddir)/libev/libev.la $(builddir)/libstatsd/src/libstatsd.la

