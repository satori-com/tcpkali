
dist_check_SCRIPTS = check-tcpkali.sh check-tcpkali.py

if HAVE_PYTHON
TESTS = check-tcpkali.sh check-tcpkali.py
else
TESTS = check-tcpkali.sh
endif

TESTS_ENVIRONMENT= TCPKALI="$(top_builddir)/src/tcpkali"

# Use `brew install shellcheck && pip install pep8 pylint`
lint:
	${SHELLCHECK} -e SC2086 *.sh
	${PEP8} *.py
	${PYLINT} --reports=n --persistent=n *.py
	${PYLINT} --reports=n --persistent=n --py3k *.py
