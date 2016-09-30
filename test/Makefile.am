
dist_check_SCRIPTS = check-tcpkali-operation.sh check-tcpkali-traffic.py

if HAVE_PYTHON
TESTS = check-tcpkali-operation.sh check-tcpkali-traffic.py
else
TESTS = check-tcpkali-operation.sh
endif

TESTS_ENVIRONMENT= TCPKALI="$(top_builddir)/src/tcpkali"

# Use `brew install shellcheck && pip install pep8 pylint`
lint:
	${SHELLCHECK} -e SC2086 *.sh
	${PEP8} *.py
	${PYLINT} --reports=n --persistent=n *.py
	${PYLINT} --reports=n --persistent=n --py3k *.py
