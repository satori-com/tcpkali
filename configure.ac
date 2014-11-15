AC_INIT(tcpkali, 0.2, lwalkin@machinezone.com)

AC_CONFIG_SRCDIR([src/tcpkali.c])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(foreign)
AC_CONFIG_MACRO_DIR(m4)

AC_CONFIG_HEADER([config.h])

AM_PROG_LIBTOOL

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

dnl Third-party dependencies.
m4_include([src/libev/libev.m4])
m4_include([src/libstatsd/libstatsd.m4])

dnl Import compile flags checking macro.
m4_include([m4/ax_check_compile_flag.m4])

dnl Global CFLAGS, for tcpkali and its third-party dependencies.
AX_CHECK_COMPILE_FLAG([-pthread], [CFLAGS="$CFLAGS -pthread"])

dnl tcpkali-specific CFLAGS.
AX_CHECK_COMPILE_FLAG([-W], [TK_CFLAGS="$TK_CFLAGS -W"])
AX_CHECK_COMPILE_FLAG([-Wall], [TK_CFLAGS="$TK_CFLAGS -Wall"])
AX_CHECK_COMPILE_FLAG([-Wno-strict-aliasing], [TK_CFLAGS="$TK_CFLAGS -Wno-strict-aliasing"])
AX_CHECK_COMPILE_FLAG([-fno-strict-aliasing], [TK_CFLAGS="$TK_CFLAGS -fno-strict-aliasing"])

dnl Apply TK_CFLAGS only for tcpkali sources, not third-party dependencies.
AC_SUBST(TK_CFLAGS)

AC_CHECK_HEADERS(sched.h)
AC_CHECK_FUNCS(sched_getaffinity)

AC_CONFIG_FILES([Makefile
                 test/Makefile
                 src/Makefile
                 src/libev/Makefile
                 src/libstatsd/Makefile
                 src/libstatsd/man/Makefile
                 src/libstatsd/src/Makefile
                ])
AC_OUTPUT
