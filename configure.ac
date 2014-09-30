AC_INIT(tcpkali, 0.1, lwalkin@machinezone.com)

AC_CONFIG_SRCDIR([tcpkali.c])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(foreign)
AC_CONFIG_MACRO_DIR(m4)

AC_CONFIG_HEADER([config.h])

AM_PROG_LIBTOOL

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

m4_include([libev/libev.m4])
m4_include([libstatsd/libstatsd.m4])
m4_include([m4/ax_check_compile_flag.m4])

AX_CHECK_COMPILE_FLAG([-W], [CFLAGS="$CFLAGS -W"])
AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="$CFLAGS -Wall"])
AX_CHECK_COMPILE_FLAG([-Wno-strict-aliasing], [CFLAGS="$CFLAGS -Wno-strict-aliasing"])
AX_CHECK_COMPILE_FLAG([-fno-strict-aliasing], [CFLAGS="$CFLAGS -fno-strict-aliasing"])
AX_CHECK_COMPILE_FLAG([-pthread], [CFLAGS="$CFLAGS -pthread"])

AC_CHECK_HEADERS(sched.h)
AC_CHECK_FUNCS(sched_getaffinity)

AC_CONFIG_FILES([Makefile
                 libev/Makefile
                 libstatsd/Makefile
                 libstatsd/man/Makefile
                 libstatsd/src/Makefile
                ])
AC_OUTPUT
