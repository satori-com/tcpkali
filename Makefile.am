
SUBDIRS = libev

bin_PROGRAMS = tcpkali

tcpkali_CPPFLAGS = -Ilibev
tcpkali_CFLAGS = -std=gnu99
tcpkali_LDFLAGS = -all-static
tcpkali_SOURCES = \
                tcpkali_dns.c tcpkali_dns.h         \
                tcpkali_engine.c tcpkali_engine.h   \
                tcpkali_signals.c tcpkali_signals.h \
                tcpkali.c tcpkali.h tcpkali_pacefier.h
tcpkali_LDADD = libev/libev.la
