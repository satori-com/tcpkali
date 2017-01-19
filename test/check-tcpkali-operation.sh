#!/bin/sh

if [ -z "${CONTINUOUS_INTEGRATION}" ]; then
    set -o pipefail
fi
set -e

if [ -z "${TCPKALI}" ]; then
    echo "WARNING: Use \`make check\` instead of running $0 directly."
    TCPKALI=../src/tcpkali
fi

use_test_no="$1"
if [ -n "$1" ]; then
    echo "Selected test ${use_test_no}"
fi

PORT=1230

check() {
    local testno="$1"
    local togrep="$2"
    shift 2

    if [ -n "${use_test_no}" -a "${testno}" != "${use_test_no}" ]; then
        return
    fi

    PORT=$((PORT+1))
    local rest_opts="-T1s --source-ip 127.1 -l${PORT} 127.1:${PORT}"
    echo "Test ${testno}.srcip: $* ${rest_opts}" >&2
    "$@" ${rest_opts} 2>&1 | egrep "$togrep"
    PORT=$((PORT+1))
    local rest_opts="-T1s -l${PORT} 127.1:${PORT}"
    echo "Test ${testno}.autoip: $* ${rest_opts}" >&2
    "$@" ${rest_opts} 2>&1 | egrep "$togrep"
}


check 1 "." ${TCPKALI} --connections=20 --duration=1
check 2 "." ${TCPKALI} --connections=10 --duration=1 -m Z
check 3 "." ${TCPKALI} -c10 --message Z --message-rate=2
check 4 "." ${TCPKALI} -c10 -m Z --channel-bandwidth-upstream=10kbps

check 5 "Total data sent:[ ]+149 bytes"     ${TCPKALI} --ws
check 6 "Total data received:[ ]+278 bytes" ${TCPKALI} --ws
check 7 "Total data sent:[ ]+158 bytes"     ${TCPKALI} --ws --first-message ABC
check 8 "Total data received:[ ]+287 bytes" ${TCPKALI} --ws --first-message ABC

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

check 24 "Packet rate estimate: (19|20)" ${TCPKALI} -m 'Foo\{message.marker}' -r10
check 25 "Packet rate estimate: (19|20)" ${TCPKALI} --ws -m '\{message.marker}\{re [a-z]{1,300}}' -r10
