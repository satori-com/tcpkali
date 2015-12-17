
# About

`tcpkali` is a high performance TCP and WebSocket load generator and sink.

![tcpkali mascot](doc/images/tcpkali-mascot.png)

# Features

 * Efficient multi-core operation (`--workers`); utilizes all available cores by default.
 * Allows opening massive number of connections (`--connections`)
 * Allows limiting an upstream and downstream of a single connection throughput (`--channel-bandwidth-downstream`, `--channel-bandwidth-upstream` or `--message-rate`)
 * Allows specifying the first and subsequent messages (`--message`, `--first-message`).
 * Measures response latency percentiles using [HdrHistogram](https://github.com/HdrHistogram) (`--latency-marker`)
 * Sends stats to StatsD/DataDog (`--statsd`)

# Quick example: testing a web server

    tcpkali -em "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n" -r 10 --latency-marker "HTTP/1.1" google.com:80

# Pre-requisites

Install the following packages first:

 * autoconf
 * automake
 * libtool
 * bison
 * flex
 * gcc-c++

# Build & Install

    test -f configure || autoreconf -iv
    ./configure
    make
    sudo make install

[![Build Status](https://travis-ci.org/machinezone/tcpkali.svg?branch=master)](https://travis-ci.org/machinezone/tcpkali)

# Usage

    Usage: tcpkali [OPTIONS] [-l <port>] [<host:port>...]

You can get the full list of options using `tcpkali --help`, from
`man tcpkali`, and by consulting the
[tcpkali man page source](doc/tcpkali.man.md).

# TCP Examples

Connect to a local web server and do nothing:

    tcpkali 127.0.0.1:80

Connect to a local echo server and hammer it with stream of dollars:

    tcpkali --message '$' localhost:echo
    tcpkali -m '$' localhost:echo

Open 10000 connections to two remote servers:

    tcpkali --connections 10000 yahoo.com:80 google.com:80
    tcpkali -c 10k yahoo.com:80 google.com:80

Open 100 connections to itself and do nothing:

    tcpkali --connections 100 --listen-port 12345 127.0.0.1:12345
    tcpkali -c100 -l12345 127.1:12345

Open a connection to itself and send lots of cookies:

    tcpkali --listen-port 12345 --message "cookies" 127.0.0.1:12345
    tcpkali -l 12345 -m "cookies" 127.1:12345

Listen for incoming connections and throw away data for 3 hours:

    tcpkali --listen-port 12345 --duration 3h
    tcpkali -l12345 -T3h

# WebSocket examples

Open connection to the local WebSocket server, send hello, and wait:

    tcpkali --websocket --first-message "hello" 127.0.0.1:80

Open connection to the local server and send tons of empty JSON frames:

    tcpkali --websocket --message "{}" 127.1:80

Open connection to the local server and send a JSON frame every second:

    tcpkali --ws -m "{}" -r1 127.1:80

