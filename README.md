
# About

`tcpkali` is a high performance TCP and WebSocket load generator and sink.

![tcpkali mascot](doc/images/tcpkali-mascot.png)

# Features

 * Opens millions of connections from a single host by using available interface aliases.
 * Efficient multi-core operation (`--workers`); utilizes all available cores by default.
 * Allows opening massive number of connections (`--connections`)
 * Allows limiting an upstream and downstream of a single connection throughput (`--channel-bandwidth-downstream`, `--channel-bandwidth-upstream` or `--message-rate`)
 * Allows specifying the first and subsequent messages (`--message`, `--first-message`).
 * Measures response latency percentiles using [HdrHistogram](https://github.com/HdrHistogram) (`--latency-marker`)
 * Sends stats to StatsD/DataDog (`--statsd`)

# Quick example: testing a web server

    tcpkali -em "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n" -r 10 \
            --latency-marker "HTTP/1.1" google.com:80

# Install

## From packages

| OS       | Package manager                         | Command                |
| -------- | --------------------------------------- | ---------------------- |
| Mac OS X | [Homebrew](http://brew.sh/)             | `brew install tcpkali` |
| Mac OS X | [MacPorts](https://www.macports.org/)   | `port install tcpkali` |
| FreeBSD  | [pkgng](https://wiki.freebsd.org/pkgng) | `pkg install tcpkali`  |
| Linux    | [nix](https://nixos.org/nix/)           | `nix-env -i tcpkali`   |

## From sources

Install the following packages first:

 * autoconf
 * automake
 * libtool
 * bison
 * flex
 * gcc-c++
 * ncurses-devel or equivalent ncurses package, *optional*.

**Build and install:**

    test -f configure || autoreconf -iv
    ./configure
    make
    sudo make install

[![Build Status](https://travis-ci.org/satori-com/tcpkali.svg?branch=master)](https://travis-ci.org/satori-com/tcpkali)

# Usage (Short version)

    Usage: tcpkali [OPTIONS] [-l <port>] [<host:port>...]
    Where some OPTIONS are:
      -h                   Print this help screen, then exit
      --help               Print long help screen, then exit
      -d                   Dump i/o data for a single connection

      -c <N>               Connections to keep open to the destinations
      -l <port>            Listen on the specified port
      --ws, --websocket    Use RFC6455 WebSocket transport
      -T <Time=10s>        Exit after the specified amount of time

      -e                   Unescape backslash-escaping in a message string
      -1 <string>          Message to send to the remote host once
      -m <string>          Message to repeatedly send to the remote
      -r <Rate>            Messages per second to send in a connection

    Variable units and recognized multipliers:
      <N>, <Rate>:  k (1000, as in "5k" is 5000), m (1000000)
      <Time>:       ms, s, m, h, d (milliseconds, seconds, minutes, hours, days)
      <Rate> and <Time> can be fractional values, such as 0.25.

You can get the full list of options using `tcpkali --help`, from
`man tcpkali`, and by consulting the
[tcpkali man page source](doc/tcpkali.man.md).

# Usage Examples
<details>
<summary>A few command line examples</summary>

## TCP Examples

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

## WebSocket examples

Open connection to the local WebSocket server, send hello, and wait:

    tcpkali --websocket --first-message "hello" 127.0.0.1:80

Open connection to the local server and send tons of empty JSON frames:

    tcpkali --websocket --message "\{ws.text}" 127.1:80

Send a binary frame with a picture every second (angle brackets are literal):

    tcpkali --ws -m "\{ws.binary <image.png>}" -r1 127.1:80

</details>
