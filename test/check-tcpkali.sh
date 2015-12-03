#!/bin/sh

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

OPTS="-T1 --source-ip 127.1"

check 1  ${TCPKALI} ${OPTS} --connections=20 --duration=1 -l1271 127.1:1271
check 2  ${TCPKALI} ${OPTS} -c10 --message Z --message-rate=1 -l1271 127.1:1271
check 3  ${TCPKALI} ${OPTS} -c10 -m Z --channel-bandwidth-upstream=10kbps -l1271 127.1:1271
check 4  ${TCPKALI} ${OPTS} --connections=10 --duration=1 -m Z -l1271 127.1:1271

check 5  ${TCPKALI} ${OPTS} --ws -l1271 127.1:1271 | egrep -q "Total data sent:[ ]+149 bytes"
check 6  ${TCPKALI} ${OPTS} --ws -l1271 127.1:1271 | egrep -q "Total data received:[ ]+278 bytes"
check 7  ${TCPKALI} ${OPTS} --ws -l1271 127.1:1271 --first-message ABC | egrep -q "Total data sent:[ ]+158 bytes"
check 8  ${TCPKALI} ${OPTS} --ws -l1271 127.1:1271 --first-message ABC | egrep -q "Total data received:[ ]+287 bytes"

check 9  ${TCPKALI} ${OPTS} --ws -l1271 127.1:1271 --message ABC
check 10 ${TCPKALI} ${OPTS} --ws -l1271 127.1:1271 --first-message ABC --message foo
