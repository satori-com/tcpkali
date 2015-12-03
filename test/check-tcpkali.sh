#!/bin/sh

set -e

if [ -z "${TCPKALI}" ]; then
    echo "WARNING: Use \`make check\` instead of running $0 directly."
    TCPKALI=../src/tcpkali
fi

PORT=1234

check() {
    local testno
    testno=$1
    shift
    echo "Test $testno: $@" >&2
    PORT=$(($PORT+1))
    $@ -T1 --source-ip 127.1 -l${PORT} 127.1:${PORT}
}

check 1  ${TCPKALI} --connections=20 --duration=1
check 2  ${TCPKALI} -c10 --message Z --message-rate=1
check 3  ${TCPKALI} -c10 -m Z --channel-bandwidth-upstream=10kbps
check 4  ${TCPKALI} --connections=10 --duration=1 -m Z

check 5  ${TCPKALI} --ws | egrep -q "Total data sent:[ ]+149 bytes"
check 6  ${TCPKALI} --ws | egrep -q "Total data received:[ ]+278 bytes"
check 7  ${TCPKALI} --ws | egrep -q "Total data sent:[ ]+158 bytes"
check 8  ${TCPKALI} --ws | egrep -q "Total data received:[ ]+287 bytes"

check 9  ${TCPKALI} --ws --message ABC
check 10 ${TCPKALI} --ws --first-message ABC --message foo
