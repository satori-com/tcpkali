#!/bin/bash

set -e

if [ -z "${TCPKALI}" ]; then
    echo "Use \`make check\` instead of running $0 directly."
    exit 1
fi

${TCPKALI} --connections=20 --duration=1 -l1271 127.1:1271
${TCPKALI} -c10 -T1 --message ' ' --message-rate=1 -l1271 127.1:1271
${TCPKALI} -c10 -T1 -m ' ' --channel-bandwidth=10kbps -l1271 127.1:1271
${TCPKALI} --connections=10 --duration=1 -m ' ' -l1271 127.1:1271
