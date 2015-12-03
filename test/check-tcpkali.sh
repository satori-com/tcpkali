#!/bin/sh

set -e

if [ -z "${TCPKALI}" ]; then
    echo "WARNING: Use \`make check\` instead of running $0 directly."
    TCPKALI=../src/tcpkali
fi

PORT=1230

check() {
    local testno=$1
    local togrep=$2
    shift 2
    PORT=$(($PORT+1))
    local rest_opts="-T1 --source-ip 127.1 -l${PORT} 127.1:${PORT}"
    echo "Test $testno: $* ${rest_opts}" >&2
    $@ ${rest_opts} | egrep "$togrep"
}

check 1 "." ${TCPKALI} --connections=20 --duration=1
check 2 "." ${TCPKALI} -c10 --message Z --message-rate=1
check 3 "." ${TCPKALI} -c10 -m Z --channel-bandwidth-upstream=10kbps
check 4 "." ${TCPKALI} --connections=10 --duration=1 -m Z

check 5 "Total data sent:[ ]+149 bytes"     ${TCPKALI} --ws
check 6 "Total data received:[ ]+278 bytes" ${TCPKALI} --ws
check 7 "Total data sent:[ ]+158 bytes"     ${TCPKALI} --ws --first-message ABC
check 8 "Total data received:[ ]+287 bytes" ${TCPKALI} --ws --first-message ABC

check 9 "." ${TCPKALI} --ws --message ABC
check 10 "." ${TCPKALI} --ws --first-message ABC --message foo
