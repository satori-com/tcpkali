#!/bin/bash
sudo tcpdump -netti lo tcp and port 12345 -A
