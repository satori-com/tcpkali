noinst_HEADERS = hdr_dbl_histogram.h hdr_histogram.h

noinst_LTLIBRARIES = libhdr_histogram.la

libhdr_histogram_la_CFLAGS = -std=gnu99
libhdr_histogram_la_SOURCES = \
	hdr_histogram.c      hdr_interval_recorder.h	\
	hdr_histogram.h      hdr_time.h	\
	hdr_dbl_histogram.c  hdr_writer_reader_phaser.h	\
	hdr_dbl_histogram.h

libhdr_histogram_la_LDFLAGS = -lm

check_hdr_histogram_SOURCES = check_hdr_histogram.c
check_hdr_histogram_CFLAGS = -std=gnu99
check_hdr_histogram_LDADD = libhdr_histogram.la

TESTS = $(check_PROGRAMS)
check_PROGRAMS = check_hdr_histogram

