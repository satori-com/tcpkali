
noinst_LTLIBRARIES = libpcg32.la

libpcg32_la_SOURCES = pcg_basic.c pcg_basic.h

check_pcg32_demo_SOURCES = pcg32-demo.c
check_pcg32x2_demo_SOURCES = pcg32x2-demo.c
check_pcg32_global_demo_SOURCES = pcg32-global-demo.c

LDADD = libpcg32.la

check_PROGRAMS = check_pcg32_demo check_pcg32x2_demo check_pcg32_global_demo
TESTS = $(check_PROGRAMS)
