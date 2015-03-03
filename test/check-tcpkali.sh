#!/bin/bash

set -e

if [ -z "${TCPKALI}" ]; then
    echo "WARNING: Use \`make check\` instead of running $0 directly."
    TCPKALI=../src/tcpkali
fi

check() {
    local testno
    testno=$1
    shift
    echo "Test $testno: $@" >&2
    $@
}

check 1  ${TCPKALI} --connections=20 --duration=1 -l1271 127.1:1271
check 2  ${TCPKALI} -c10 -T1 --message Z --message-rate=1 -l1271 127.1:1271
check 3  ${TCPKALI} -c10 -T1 -m Z --channel-bandwidth=10kbps -l1271 127.1:1271
check 4  ${TCPKALI} --connections=10 --duration=1 -m Z -l1271 127.1:1271

check 4  ${TCPKALI} -T1s --ws -l1271 127.1:1271 | egrep -q "Total data sent:\s+149 bytes"
check 6  ${TCPKALI} -T1s --ws -l1271 127.1:1271 | egrep -q "Total data received:\s+278 bytes"
check 7  ${TCPKALI} -T1s --ws -l1271 127.1:1271 --first-message ABC | egrep -q "Total data sent:\s+158 bytes"
check 8  ${TCPKALI} -T1s --ws -l1271 127.1:1271 --first-message ABC | egrep -q "Total data received:\s+287 bytes"

check 9  ${TCPKALI} -T1s --ws -l1271 127.1:1271 --message ABC
check 10 ${TCPKALI} -T1s --ws -l1271 127.1:1271 --first-message ABC --message foo
