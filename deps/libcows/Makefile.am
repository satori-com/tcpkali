noinst_LTLIBRARIES = libcows.la

libcows_la_SOURCES =    \
    buffers.c libcows_base64.c libcows_frame.h \
    buffers.h libcows_base64.h sha-1.c \
    libcows.c libcows_common.h libcows_frame.c \
    libcows.h

check_buffers_SOURCES = buffers.c
check_buffers_CFLAGS = -DUNIT_TEST_BUFFERS $(AM_CFLAGS)

check_sha_1_SOURCES = sha-1.c
check_sha_1_CFLAGS = -DUNIT_TEST_SHA1 $(AM_CFLAGS)

check_PROGRAMS = check_buffers check_sha_1
TESTS = $(check_PROGRAMS)

