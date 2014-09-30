
SUBDIRS = libev libstatsd

bin_PROGRAMS = tcpkali

tcpkali_CPPFLAGS = -Ilibev -Ilibstatsd/src
tcpkali_CFLAGS = -std=gnu99
tcpkali_LDFLAGS = -all-static
tcpkali_SOURCES = \
                tcpkali_dns.c tcpkali_dns.h               \
                tcpkali_engine.c tcpkali_engine.h         \
                tcpkali_syslimits.c tcpkali_syslimits.h   \
                tcpkali_signals.c tcpkali_signals.h       \
                tcpkali_pacefier.h tcpkali_atomic.h       \
                tcpkali.c tcpkali.h
tcpkali_LDADD = libev/libev.la libstatsd/src/libstatsd.la
