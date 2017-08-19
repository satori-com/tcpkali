% tcpkali(1) TCPKali user manual
% Lev Walkin <lwalkin@machinezone.com>
% 2017-01-20

# NAME

tcpkali -- fast TCP and WebSocket load generator and sink.

# SYNOPSIS

tcpkali [*OPTIONS*] [**-l** *port*] [*host:port* ...]

# DESCRIPTION

tcpkali is a tool that helps stress-test and bench TCP and WebSocket based
systems. In the *client* mode tcpkali connects to the list of specified
hosts and ports and generates traffic for each of these connections. In the
*server* mode tcpkali accepts incoming connections and throws away any
incoming data.

tcpkali can throw unlimited or bandwidth-controlled traffic to the
remote destinations both in the *client* and in the *server* mode.

The *client* mode is triggered by specifying one or more *host:port* arguments
on the command line. The *server* mode is triggered by specifying **-l** (**--listen-port** *port*).

# OPTIONS
## GENERAL OPTIONS

-h, --help
:   Print a help screen, then exit.

--version
:   Print version number, then exit.

-v, --verbose *level*
:   Increase (**-v**) or set (**--verbose**) output verbosity level [0..3]. Default is 1.

-d, --dump-one
: Dump input and output data on a single arbitrarily chosen connection.
When connection gets closed, some other connection is used for dumping.

--dump-one-in
: Dump only the input data on a single connection.

--dump-one-out
: Dump only the output data on a single connection.

--dump-{all,all-in,all-out}
: Dump input and/or output data on *all* connections.

--write-combine=off
:   Send messages individually instead of batching writes. Implies **--nagle=off**, if not overriden by the command line. Default is `on`.

-w, --workers *N*
:   Number of parallel threads to use. Default is to use as many as needed,
    up to the number of cores detected in the system.

## NETWORK STACK SETTINGS

--nagle=on|off
:   Control Nagle algorithm by setting `TCP_NODELAY` socket option using **setsockopt**(). Default is not to call **setsockopt**() at all, which leaves Nagle *enabled* on most systems.

--rcvbuf *SizeBytes*
:   Set TCP receive buffers (set `SO_RCVBUF` socket option using **setsockopt**()). This option has no effect on some systems with automatic receive buffer management. tcpkali will print a message if **--rcvbuf** has no effect.

--sndbuf *SizeBytes*
:   Set TCP send buffers (set `SO_SNDBUF` socket option using **setsockopt**()). This option has no effect on some systems with automatic receive buffer management. tcpkali will print a message if **--sndvbuf** has no effect.

--source-ip *IP*
:   By default, tcpkali automatically detects and uses all interface aliases
to connect to destination hosts. This default behavior allows tcpkali to
open more than 64k connections to destinations.

    Use the **--source-ip** to override this behavior
    by specifying a particular source IP to use.
    Specifying **--source-ip** option multiple times builds
    a list of source IPs to use.

## TEST RUN OPTIONS

--ws, --websocket
:   Use RFC6455 WebSocket transport.

--ssl
:   Enable Transport Layer Security (TLS, formerly known as SSL) for client-side and server-side connections.

--ssl-cert *filename*
:   The X.509 certificate file for TLS termination. Default is "cert.pem".

--ssl-key *filename*
:   The private key file for TLS termination. Default is "key.pem".

-H, --header
:   Add HTTP header into the WebSocket handshake.

-c, --connections *N*
:   Number of concurrent connections to open to the destinations. Default is 1.

--connect-rate *Rate*
:   Limit number of new connections per second.
    Default is 100 connections per second.

--connect-timeout *Time*
:   Limit time spent in a connection attempt. Default is 1 second.

--channel-lifetime *Time*
:   Shut down each connection after *Time* seconds.

--channel-bandwidth-upstream *Bandwidth*
:   Limit single connection bandwidth in the outgoing direction.

--channel-bandwidth-downstream *Bandwidth*
:   Limit single connection bandwidth in the incoming direction.

-l, --listen-port *port*
:   Accept connections on the specified port.

--listen-mode=silent|active
:   How to behave when a new client connection is received. In the `silent` mode we do not send data and ignore the data received. This is a default. In the `active` mode tcpkali sends messages to the connected clients.

-T, --duration *Time*
:   Exit and print final stats after the specified amount of time. Default is 10 seconds (`-T10s`).

--delay-send *Time*
:   Delay sending bytes by a specified amount of time.

## TRAFFIC CONTENT OPTIONS

-e, --unescape-message-args
:   Unescape the message data specified using the **-m**, **-f**
    and the rest of the traffic content options on the command line.
    Transforms \\xxx sequences into bytes with the corresponding octal values,
    \\n into a newline character, etc.

-1, --first-message <string>
:   Send this message first, once at the beginning of each connection.
    This option can be specified several times to send several initial
    messages at the beginning of each connection. If **--websocket** option
    is given, each message is wrapped into its own WebSocket frame.

--first-message-file *filename*
:   Read the message from a file and send it once at the beginning of each connection.
    This option can be specified several times.

-m, --message *string*
:   Repeatedly send the specified message to each destination.
    This option can be specified several times.

--message-stop *string*
:   Terminate tcpkali if the given string is encountered in the incoming byte stream.

-f, --message-file *filename*
:   Repeatedly send the message read from the file to each destination.
    This option can be specified several times.

-r, --message-rate *Rate*
:   Messages per second to send in a connection. tcpkali attempts to preserve
    message boundaries. This setting is mutually incompatible with the
    **--channel-bandwidth-upstream** option, because they both control
    the message sending rate.

-r, --message-rate @*Latency*
:   Instead of specifying the message rate, attempt to figure out the
    maximum message rate that does not result in exceeding the given
    message latency. Requires **--latency-marker** option to be set.

    EXAMPLE: tcpkali **-m** "PING" **--latency-marker** "PONG" -r **@100ms**

### Traffic content expressions

tcpkali supports injecting a limited form of variability into the
generated content. All message data, be it the **-m** or **--first-message**,
can contain the dynamic expressions in the form of "\\{EXPRESSION}".

-----------------------------------------------------------------------
Expression          Description
--------------      ---------------------------------------------------
 connection.uid     Unique number incremented for each new connection.

 connection.ptr     Pointer to a connection structure. Don't use.

 connection.re      Randomized expression, unique per connection.

 global.re          Randomized expression, same across all connections.

 re                 Randomized expression, for each message.

 message.marker     Produce a message timestamp for message rate and latency
                    measurements.

 ws.continuation,   Specify WebSocket frame types.
 ws.ping, ws.pong,  Refer to RFC 6455, section 11.8.
 ws.text, ws.binary

 EXPRESSION % *int* Remainder of the expression value divided by *int*.
-----------------------------------------------------------------------
: Expressions can be of the following forms:

Expressions can be used to provide some amount of variability to the
outgoing data stream. For example, the following command line might be used to
load 10 different resources from an HTTP server:

tcpkali **-em** `'GET /image-\{connection.uid%10}.jpg\r\n\r\n'` ...

The following command is used to come up with random alphanumeric identifiers:

tcpkali **-em** `'GET /image-\{re [a-z0-9]+}.jpg\r\n\r\n'` ...

Expressions are evaluated even if the **-e** option is not given.

## LATENCY MEASUREMENT OPTIONS

tcpkali can measure TCP connect latency, time to first byte, and
request-response latencies.

--latency-connect
: Measure TCP connect latency.

--latency-first-byte
: Measure latency to first byte. Works only for the active sockets.

tcpkali measures request-response latency by repeatedly recording
the time difference between the time the message is sent
(as specified by **-m** or **-f**)
and the time the latency marker is observed in the downstream traffic
(as set by **--latency-marker**).

--latency-marker *string*
:   Specify a per-message sequence of characters to look for in the data stream.

--latency-marker-skip *N*
:   Ignore the first *N* observations of a **--latency-marker**.

--latency-percentiles *list*
:   Report latency at specified percentiles.
    The option takes a comma-separated list of floating point values.
    Mean and maximum values can be reported using **--latency-percentiles 50,100**.
    Default is `95,99,99.5`.

--message-marker
:   Passive mode detection or message markers. Given this option, tcpkali
    will detect the \\{message.marker} byte sequences and will calculate
    message rate (in messages per second) and message arrival latency.
    In the active mode, message rate calculation is implicitly enabled by
    using the \\{message.marker} expression.

## STATSD OPTIONS

--statsd
:   Enable StatsD output. StatsD output is disabled by default.

--statsd-host *host*
:   StatsD host to send metrics data to. Default is `localhost`.

--statsd-port *port*
:   StatsD port to use. Default is 8125.

--statsd-namespace *string*
:   Metric namespace. Default is "tcpkali".

--statsd-latency-window *Time*
:   By default latencies are measured across the entire duration of
    tcpkali's run (as set by **--duration** or **-T**).
    This option instructs tcpkali to flush latency data to StatsD every *Time*
    period and start measuring latencies anew.
    The latencies that are displayed in the user interface remain being
    collected across the whole run.

# VARIABLE UNITS

-----------------------------------------------------------------------
Placeholder       Recognized unit suffixes
----------------  -----------------------------------------------------
*N* and *Rate*    k (1000, as in "5k" equals to 5000), m (1000000).

*SizeBytes*       k (1024, as in "5k" equals to 5120), m (1024*1024).

*Bandwidth*       kbps, Mbps (for bits per second),
                  kBps,\ MBps\ (for\ bytes\ per\ second).

*Time*, *Latency* ms, s, m, h, d (milliseconds, seconds, etc).
-----------------------------------------------------------------------
Table: tcpkali recognizes a number of suffixes for numeric values.

*Rate*, *Time* and *Latency* can be fractional values, such as 0.25.

# EXAMPLES

 1. Throw 42 requests per second (**-r**) in each of the 10,000 connections (**-c**) to an HTTP server (**-m**), replacing \\n with newlines (**-e**):

    tcpkali -c10k -r42 -em 'GET / HTTP/1.0\\r\\n\\r\\n' nonexistent.com:80

 2. Create a WebSocket (**--ws**) server on a specifed port (**-l**) for an hour (**-T**), but block clients from actually sending data:

    tcpkali --ws -l8080 --channel-bandwidth-downstream=0 -T1h

 3. Show server responses (**--verbose**) when we ping SMTP server once a second (**--connect-rate**) disconnecting promptly (**--channel-lifetime**):

    tcpkali --connect-rate=1 --channel-lifetime=0.1 -vvv nonexistent.org:smtp

# SEE ALSO

## Sysctls to tune the system to be able to open more connections

...for N connections, such as 50k:

    kern.maxfiles=10000+2*N         # BSD
    kern.maxfilesperproc=100+2*N    # BSD
    kern.ipc.maxsockets=10000+2*N   # BSD
    fs.file-max=10000+2*N           # Linux
    net.ipv4.tcp_max_orphans=N      # Linux

    # For load-generating clients.
    net.ipv4.ip_local_port_range="10000  65535"  # Linux.
    net.inet.ip.portrange.first=10000  # BSD/Mac.
    net.inet.ip.portrange.last=65535   # (Enough for N < 55535)
    net.ipv4.tcp_tw_reuse=1         # Linux
    net.inet.tcp.maxtcptw=2*N       # BSD

    # If using netfilter on Linux:
    net.netfilter.nf_conntrack_max=N
    echo $((N/8)) > /sys/module/nf_conntrack/parameters/hashsize

## Readings

* On TIME-WAIT state and its reuse:\
http://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux.html

* On netfliter settings:\
http://serverfault.com/questions/482480/

