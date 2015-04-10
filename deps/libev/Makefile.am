AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = LICENSE Changes libev.m4 \
	     ev_vars.h ev_wrap.h \
	     ev_epoll.c ev_select.c ev_poll.c ev_kqueue.c ev_port.c ev_win32.c \
	     ev.3 ev.pod Symbols.ev Symbols.event

noinst_MANS = ev.3

noinst_HEADERS = ev.h ev++.h event.h

noinst_LTLIBRARIES = libev.la

libev_la_SOURCES = ev.c event.c

ev.3: ev.pod
	pod2man -n LIBEV -r "libev-$(VERSION)" -c "libev - high performance full featured event loop" -s3 <$< >$@
