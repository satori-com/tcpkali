#!/bin/bash

set -e

./tcpkali --connections=20 --duration=1 -l1271 127.1:1271
./tcpkali -c10 -T1 --message ' ' --message-rate=1 -l1271 127.1:1271
./tcpkali -c10 -T1 -m ' ' --channel-bandwidth=10kbps -l1271 127.1:1271
./tcpkali --connections=10 --duration=1 -m ' ' -l1271 127.1:1271
