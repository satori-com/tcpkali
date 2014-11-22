noinst_LTLIBRARIES = libcows.la

libcows_la_SOURCES = libcows.c libcows_frame.c libcows_base64.c buffers.c

check_buffers_SOURCES = buffers.c
check_buffers_CFLAGS = -DUNIT_TEST_BUFFERS $(AM_CFLAGS)

check_sha_1_SOURCES = sha-1.c
check_sha_1_CFLAGS = -DUNIT_TEST_SHA1 $(AM_CFLAGS)

check_PROGRAMS = check_buffers check_sha_1
TESTS = $(check_PROGRAMS)

