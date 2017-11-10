#!/usr/bin/env bash

set -o pipefail
set -e

if [ -z "${TCPKALI}" ]; then
    echo "WARNING: Use \`make check\` instead of running $0 directly."
    TCPKALI=../src/tcpkali
fi

use_test_no="$1"
if [ -n "$1" ]; then
    echo "Selected test ${use_test_no}"
fi

TMPFILE=/tmp/check-tcpkali-output.$$
rmtmp() {
    local lines
    lines=$(wc -l ${TMPFILE} | awk '{print $1}')
    if [ ${lines} -gt 150 ]; then
        echo "First 50 lines of tcpkali output (total ${lines}):"
        head -50 ${TMPFILE}
        echo "Last 50 lines of tcpkali output (total ${lines}):"
        tail -50 ${TMPFILE}
    else
        echo "All ${lines} lines of tcpkali output:"
        cat ${TMPFILE}
    fi
    echo "Removing temporary file ${TMPFILE}."
    rm -f ${TMPFILE}
}
trap rmtmp EXIT
touch ${TMPFILE}

PORT=1230

check() {
    local testno="$1"
    local togrep="$2"
    local command="$3"
    shift 3

    if [ -n "${use_test_no}" ] && [ "${testno}" != "${use_test_no}" ]; then
        return
    fi

    PORT=$((PORT+1))
    local rest_opts
    rest_opts="-T1s -l127.1:${PORT} 127.1:${PORT}"
    echo "Test ${testno}.autoip: $* ${rest_opts}" | tee ${TMPFILE} >&2
    echo "Looking for \"$togrep\" in '${command} ${rest_opts} $*'" >> ${TMPFILE}
    local out
    out=$(${command} ${rest_opts} "$@" 2>&1 | tee -a ${TMPFILE} | grep -E -c "$togrep")
    [ $out -ne 0 ] || exit 1
}

check 1 "." ${TCPKALI} -vv --connections=20 --duration=1
check 2 "." ${TCPKALI} -vv --connections=10 --duration=1 -m Z
check 3 "." ${TCPKALI} -vv -c10 --message Z --message-rate=2
check 4 "." ${TCPKALI} -vv -c10 -m Z --channel-bandwidth-upstream=10kbps

check 5 "Total data sent:[ ]+149 bytes"     ${TCPKALI} -vv --ws -w2
check 6 "Total data received:[ ]+278 bytes" ${TCPKALI} -vv --ws -w2
check 7 "Total data sent:[ ]+158 bytes"     ${TCPKALI} -vv --ws --first-message ABC -w2
check 8 "Total data received:[ ]+287 bytes" ${TCPKALI} -vv --ws -1 ABC -w2

check 9 "." ${TCPKALI} --ws --message ABC
check 10 "." ${TCPKALI} --ws --first-message ABC --message foo

check 11 "latency at percentiles.*50.0/100.0" ${TCPKALI} --latency-connect --latency-first-byte --latency-percentiles 50.0,100.0
check 12 "50/100" ${TCPKALI} --latency-connect --latency-first-byte --latency-percentiles 50/100
check 13 "50/100" ${TCPKALI} --latency-connect --latency-first-byte --latency-percentiles 50 --latency-percentiles 100

check 14 "." ${TCPKALI} -m '\{ws.binary}'
check 15 "." ${TCPKALI} -m '\{ws.binary "explicit data"}'
check 16 "." ${TCPKALI} -m '\{ws.binary </dev/null>}'
check 17 "." ${TCPKALI} -m '\{ws.binary < "/dev/null" >}'
check 18 "." ${TCPKALI} -m '\{ws.binary < /dev/null >}'

check 19 "." ${TCPKALI} -m '\{connection.uid%10}'
check 20 "\[PFX-1\]" ${TCPKALI} -r3 -m 'PFX-\{connection.uid%10}' -d
check 21 "." ${TCPKALI} -c10 -m '\{re [a-z]+}'

# Test 22
TESTFILE=/tmp/.tcpkali-64k-test.$$
rm_testfile() {
    rm -f "${TESTFILE}"
}
trap rm_testfile EXIT
for size_k in 63 65 1000; do
    dd if=/dev/zero of=${TESTFILE} bs=1024 count=${size_k}
    check 22 "." ${TCPKALI} --ws -r1 -m "\{ws.text <${TESTFILE}>}"
    check 23 "." ${TCPKALI}      -r1 -m "\{ws.text <${TESTFILE}>}"
done
rm_testfile

check 24 "Packet rate estimate: (10)" ${TCPKALI} -m 'Foo\{message.marker}' -r10 --duration=5s
check 25 "Packet rate estimate: (10)" ${TCPKALI} --ws -m '\{message.marker}\{re [a-z]{1,300}}' -r10 --duration=5s

check 26 "." ${TCPKALI} -1 '\{message.marker}' -m '\{message.marker}'

trap 'rm -f ${TMPFILE}' EXIT
