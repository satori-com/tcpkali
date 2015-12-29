
dist_check_SCRIPTS = check-tcpkali.sh check-tcpkali.py

if HAVE_PYTHON
TESTS = check-tcpkali.sh check-tcpkali.py
else
TESTS = check-tcpkali.sh
endif

TESTS_ENVIRONMENT= TCPKALI="$(top_builddir)/src/tcpkali"

