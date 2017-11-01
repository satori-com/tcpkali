AC_INIT([tcpkali],[1.2.0],[lwalkin@machinezone.com])

AC_CONFIG_SRCDIR([src/tcpkali.c])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(foreign)
AC_CONFIG_MACRO_DIR(m4)

AC_CONFIG_HEADER([config.h])

LT_INIT

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AM_PROG_LEX

AX_PTHREAD([])
dnl Global CFLAGS, for tcpkali and its third-party dependencies.

CC="${PTHREAD_CC}"
LIBS="${PTHREAD_LIBS} ${LIBS}"
CFLAGS="${CFLAGS} ${PTHREAD_CFLAGS}"

AC_CHECK_PROGS([YACC], ['bison -y' byacc])
AS_IF([test "x${YACC}" == "x"], [AC_MSG_ERROR([
    ${PACKAGE_NAME} requires bison or byacc, plain yacc doesn't count.
    Install 'bison' package first.])])

dnl Third-party dependencies.
m4_include([deps/libev/libev.m4])
m4_include([deps/libstatsd/libstatsd.m4])

dnl Import locally defined macros.
m4_include([m4/ax_check_compile_flag.m4])
m4_include([m4/ax_check_openssl.m4])
m4_include([m4/ax_make_optional_include.m4])

AX_CHECK_GNU_STYLE_OPTIONAL_INCLUDE()

dnl tcpkali-specific CFLAGS.
AX_CHECK_COMPILE_FLAG([-W], [TK_CFLAGS="$TK_CFLAGS -W"])
AX_CHECK_COMPILE_FLAG([-Wall], [TK_CFLAGS="$TK_CFLAGS -Wall"])
AX_CHECK_COMPILE_FLAG([-Wno-strict-aliasing], [TK_CFLAGS="$TK_CFLAGS -Wno-strict-aliasing"])
AX_CHECK_COMPILE_FLAG([-fno-strict-aliasing], [TK_CFLAGS="$TK_CFLAGS -fno-strict-aliasing"])

dnl Apply TK_CFLAGS only for tcpkali sources, not third-party dependencies.
AC_SUBST(TK_CFLAGS)

AC_CHECK_SIZEOF([size_t])

AC_CHECK_HEADERS(sched.h uv.h)
AC_CHECK_FUNCS(sched_getaffinity)
AC_CHECK_FUNCS(sysctlbyname)
AC_CHECK_FUNCS(srandomdev)

AC_ARG_WITH([libuv],
    [AS_HELP_STRING([--with-libuv],
      [Enable support for libuv (2-4% slower than libev)])],
    [],
    [with_libuv=no])
  LIBUV=
  AS_IF([test "x$with_libuv" != xno],
    [AC_CHECK_LIB([uv], [uv_now],
      [AC_SUBST([LIBUV], ["-luv"])
       AC_DEFINE([HAVE_LIBUV], [1],
                 [Define if you have libuv])
      ],
      [AC_MSG_FAILURE(
         [--with-libuv was given, but test for libuv failed])],
      [])])

AC_CHECK_HEADERS(curses.h term.h termios.h)
AC_CHECK_LIB([ncurses], [tgetent])

dnl Enable Address Sanitizer, if supported by gcc (4.8+) or clang.
dnl http://clang.llvm.org/docs/AddressSanitizer.html
dnl https://code.google.com/p/address-sanitizer/wiki/HowToBuild
AC_ARG_ENABLE([asan],
    [AS_HELP_STRING([--enable-asan], [Enable Address Sanitizer])],
    [enable_asan=$enableval], [enable_asan=no])
  AS_IF([test "x$enable_asan" != xno], [
    AX_CHECK_COMPILE_FLAG([-fsanitize=address],
                          [CFLAGS="$CFLAGS -fsanitize=address"],
                          [enable_asan=no], [${TK_CFLAGS}])
    AS_IF([test "x$enable_asan" = xno], [
        AC_MSG_FAILURE(
            [--enable-asan is given, but not supported on this platform.
 Check out https://code.google.com/p/address-sanitizer/wiki/HowToBuild])])
    dnl Keep error messages nice. Also consider:
    dnl export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer
    dnl export ASAN_OPTIONS=symbolize=1
    AX_CHECK_COMPILE_FLAG([-fno-omit-frame-pointer],
                          [CFLAGS="$CFLAGS -fno-omit-frame-pointer"],
                          [], [${TK_CFLAGS}])
  ])

dnl Enable Thread Sanitizer, if supported by clang.
dnl http://clang.llvm.org/docs/ThreadSanitizer.html
dnl https://code.google.com/p/address-sanitizer/wiki/HowToBuild
AC_ARG_ENABLE([tsan],
    [AS_HELP_STRING([--enable-tsan], [Enable Thread Sanitizer])],
    [enable_tsan=$enableval], [enable_tsan=no])
  AS_IF([test "x$enable_tsan" != xno], [
    AX_CHECK_COMPILE_FLAG([-fsanitize=thread],
                          [CFLAGS="$CFLAGS -fsanitize=thread"],
                          [enable_tsan=no], [${TK_CFLAGS}])
    AS_IF([test "x$enable_tsan" = xno], [
        AC_MSG_FAILURE(
            [--enable-tsan is given, but not supported on this platform.
 Check out https://code.google.com/p/address-sanitizer/wiki/HowToBuild])])
    dnl Keep error messages nice.
    AX_CHECK_COMPILE_FLAG([-fno-omit-frame-pointer],
                          [CFLAGS="$CFLAGS -fno-omit-frame-pointer"],
                          [], [${TK_CFLAGS}])
    dnl -fsanitize=thread linking must be done with -pie or -shared
    dnl We can't do anything about this message in GCC; use clang.
  ])

AX_CHECK_OPENSSL(
    [AC_DEFINE([HAVE_OPENSSL], [1], [Define to 1 if you have OpenSSL])
TK_LIBS="${OPENSSL_LIBS}"
TK_LDFLAGS="${OPENSSL_LDFLAGS}"
TK_CPPFLAGS="${OPENSSL_INCLUDES}"
AC_CHECK_FUNC(SSL_CTX_set_ecdh_auto,
        [AC_DEFINE([HAVE_SSL_CTX_SET_ECDH_AUTO], [1],
            [Define to 1 if you have SSL_CTX_set_ecdh_auto function defined in OpenSSL])])
    ],
    [])
AC_SUBST(TK_LIBS)
AC_SUBST(TK_LDFLAGS)
AC_SUBST(TK_CPPFLAGS)

dnl We use pandoc to generate documentation.
AC_PATH_PROG([PANDOC], pandoc)
AM_CONDITIONAL([HAVE_PANDOC], [test -n "$PANDOC"])

dnl Use clang-format to standardize on a style.
AC_PATH_PROG([CLANG_FORMAT], clang-format, [:])
AM_CONDITIONAL([HAVE_CLANG_FORMAT], [test "${CLANG_FORMAT}" != ":"])

dnl Python is used for tests.
AM_PATH_PYTHON(,, [:])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])

dnl Python linters
AC_PATH_PROG([PEP8], pep8, [:])
AC_PATH_PROG([PYLINT], pylint, [:])

dnl Shell linters
AC_PATH_PROG([SHELLCHECK], shellcheck, [:])

AC_CONFIG_FILES([Makefile
                 asn1/Makefile
                 test/Makefile
                 src/Makefile
                 doc/Makefile
                 deps/Makefile
                 deps/libev/Makefile
                 deps/libcows/Makefile
                 deps/libstatsd/Makefile
                 deps/libstatsd/man/Makefile
                 deps/libstatsd/src/Makefile
                 deps/pcg-c-basic/Makefile
                 deps/HdrHistogram/Makefile
                 deps/boyer-moore-horspool/Makefile
                ])
AC_OUTPUT
