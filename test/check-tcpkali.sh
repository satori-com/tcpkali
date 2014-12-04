#!/bin/bash

set -e

if [ -z "${TCPKALI}" ]; then
    echo "WARNING: Use \`make check\` instead of running $0 directly."
    TCPKALI=../src/tcpkali
fi

check() {
    if [ -z "$testno" ]; then testno=0; fi
    testno=$(($testno+1))
    echo "Test $testno: $@"
    $@
}

check ${TCPKALI} --connections=20 --duration=1 -l1271 127.1:1271
check ${TCPKALI} -c10 -T1 --message Z --message-rate=1 -l1271 127.1:1271
check ${TCPKALI} -c10 -T1 -m Z --channel-bandwidth=10kbps -l1271 127.1:1271
check ${TCPKALI} --connections=10 --duration=1 -m Z -l1271 127.1:1271
