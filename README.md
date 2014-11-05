
[![Build Status](https://travis-ci.org/machinezone/tcpkali.svg?branch=master)](https://travis-ci.org/machinezone/tcpkali)

# Build

    autoreconf -iv
    ./configure
    make

# Install

    make install

# Usage

    Usage: tcpkali [OPTIONS] <host:port> [<host:port>...]
    Where OPTIONS are:
      -h, --help                  Print this help screen, then exit
      --version                   Print version number, then exit
      --verbose <level=1>         Verbosity level [0..2]
      -c, --connections <N=1>     Connections to keep open to the destinations
      -r, --connect-rate <R=100>  Limit number of new connections per second
      --connect-timeout <T=1s>    Limit time spent in a connection attempt
      --channel-lifetime <T>      Shut down each connection after T seconds
      --channel-bandwidth <Bw>    Limit single connection bandwidth
      --first-message <string>    Send this message first, once
      --first-message-file <name> Read the first message from a file
      -m, --message <string>      Message to repeatedly send to the remote
      -f, --message-file <name>   Read message to send from a file
      --message-rate <R>          Messages per second per connection to send
      -l, --listen-port <port>    Listen on the specified port
      -w, --workers <N>           Number of parallel threads to use
      -T, --duration <T=10s>      Load test for the specified amount of time
      --statsd                    Enable StatsD output (default disabled)
      --statsd-host <host>        StatsD host to send data (default is localhost)
      --statsd-port <port>        StatsD port to use (default is 8125)
      --statsd-namespace <string> Metric namespace (default is "tcpkali")
      --ws, --websocket           Use RFC6455 WebSocket transport
    And variable multipliers are:
      <R>:  k (1000, as in "5k" is 5000)
      <Bw>: kbps, Mbps (bits per second), kBps, MBps (bytes per second)
      <T>:  ms, s, m, h, d (milliseconds, seconds, minutes, hours, days)

# Examples

## Connect to a local web server and do nothing

    tcpkali 127.0.0.1:80

## Connect to a local echo server and hammer it with stream of dollars

    tcpkali --message '$' localhost:echo
    tcpkali -m '$' localhost:echo

## Open 10000 connections to two remote servers:

    tcpkali --connections -c 10000 yahoo.com:80 google.com:80
    tcpkali -c 10000 yahoo.com:80 google.com:80

## Open 100 connections to itself and do nothing.

    tcpkali --connections=100 --listen-port=12345 127.0.0.1:12345
    tcpkali -c100 -l12345 127.0.0.1:12345

## Open a connection to itself and send lots of cookies

    tcpkali --listen-port=12345 --message "cookies" 127.0.0.1:12345
    tcpkali -l 12345 -m "cookies" 127.0.0.1:12345

## WebSocket connections

### Open connection to the local WebSocket server, send hello, and wait

    tcpkali --websocket --first-message "hello" 127.0.0.1:80

### Open connection to the local server and send tons of empty JSON frames

    tcpkali --websocket --message "{}" 127.0.0.1:80

### Open connection to the local server and send a JSON frame every second

    tcpkali --websocket --message "{}" --message-rate=1 127.0.0.1:80


# Sysctls for high load
## (for N connections, such as 50k)

    kern.maxfiles=10000+2*N         # BSD
    kern.maxfilesperproc=100+N      # BSD
    fs.file-max=10000+2*N           # Linux

    # For load-generating clients.
    net.ipv4.ip_local_port_range="10000  65535"  # Linux.
    net.inet.ip.portrange.hifirst=10000          # BSD/Mac.
    net.inet.ip.portrange.hilast=65535           # (Enough for N < 55535)
    net.ipv4.tcp_tw_reuse=1

    # If using netfilter on Linux:
    net.netfilter.nf_conntrack_max=N
    net.netfilter.nf_conntrack_max=N/4

# Readings

 * On TIME-WAIT state and its reuse:
     http://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux.html
 * On netfliter settings:
     http://serverfault.com/questions/482480/

