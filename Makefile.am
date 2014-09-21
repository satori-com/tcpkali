
SUBDIRS = libev

bin_PROGRAMS = tcpkali

tcpkali_CPPFLAGS = -Ilibev
tcpkali_CFLAGS = -std=gnu99
tcpkali_SOURCES = tcpkali.c tcpkali_dns.c tcpkali_engine.c
tcpkali_LDADD = libev/libev.la
