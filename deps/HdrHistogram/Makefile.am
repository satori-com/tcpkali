noinst_HEADERS = hdr_dbl_histogram.h hdr_histogram.h hdr_histogram_log.h

noinst_LTLIBRARIES = libhdr_histogram.la

libhdr_histogram_la_CFLAGS = -std=gnu99
libhdr_histogram_la_SOURCES = \
	hdr_histogram.c      hdr_interval_recorder.h	\
	hdr_histogram.h      hdr_time.h	\
	hdr_dbl_histogram.c  hdr_histogram_log.c  hdr_writer_reader_phaser.h	\
	hdr_dbl_histogram.h  hdr_histogram_log.h

libhdr_histogram_la_LDFLAGS = -lm -lz
