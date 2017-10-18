#ifndef TCPKALI_CONNECTION_H
#define TCPKALI_CONNECTION_H

#include <sys/queue.h>
#include <sys/types.h>

#include "config.h"

#include "tcpkali_events.h"
#include "tcpkali_iface.h"
#include "tcpkali_pacefier.h"
#include "tcpkali_rate.h"
#include "tcpkali_ssl.h"
#include "tcpkali_traffic_stats.h"
#include "tcpkali_transport.h"

/*
 * A single connection is described by this structure (about 150 bytes).
 */
struct connection {
    tk_io watcher;
    tk_timer timer;
    off_t write_offset;
    struct transport_data_spec data;
    non_atomic_traffic_stats traffic_ongoing;  /* Connection-local numbers */
    size_t avg_message_size;
    size_t bytes_leftovers;
    non_atomic_traffic_stats traffic_reported; /* Reported to worker */
    float channel_eol_point; /* End of life time, since epoch */
    struct pacefier send_pace;
    struct pacefier recv_pace;
    bandwidth_limit_t send_limit;
    bandwidth_limit_t recv_limit;
    struct message_collection message_collection;
    enum {
        CW_READ_INTEREST = 0x01,
        CW_READ_BLOCKED = 0x10,
        CW_WRITE_INTEREST = 0x02,
        CW_WRITE_BLOCKED = 0x20,
        CW_WRITE_DELAYED = 0x40,
    } conn_wish : 8;
    enum conn_type {
        CONN_OUTGOING,
        CONN_INCOMING,
        CONN_ACCEPTOR,
    } conn_type : 2;
    enum conn_state {
        CSTATE_CONNECTED,
        CSTATE_CONNECTING,
    } conn_state : 1;
    enum {
        WSTATE_SENDING_HTTP_UPGRADE,
        WSTATE_WS_ESTABLISHED,
    } ws_state : 1;
    int16_t remote_index;                     /* \x ->
                                                 loop_arguments.params.remote_addresses.addrs[x] */
    non_atomic_narrow_t connection_unique_id; /* connection.uid */
    TAILQ_ENTRY(connection) hook;
    struct sockaddr_storage peer_name; /* For CONN_INCOMING */
    /* Latency */
    struct {
        double connection_initiated;
        struct ring_buffer *sent_timestamps;
        struct hdr_histogram *marker_histogram;
        unsigned message_bytes_credit; /* See (EXPL:1) below. */
        unsigned lm_occurrences_skip;  /* See --latency-marker-skip */
        /* Boyer-Moore-Horspool substring search algorithm data */
        struct StreamBMH *sbmh_marker_ctx;
        /* The following fields might be shared across connections. */
        int sbmh_shared;
        struct StreamBMH_Occ *sbmh_occ;
        const uint8_t *sbmh_data;
        size_t sbmh_size;
        struct message_marker_parser_state {
            enum { MP_DISENGAGED, MP_SLURPING_DIGITS } state;
            uint64_t collected_digits;
        } marker_parser;
    } latency;
    struct StreamBMH *sbmh_stop_ctx;
    enum {
        CBLOCKED_ON_INIT  = 0x01,
        CBLOCKED_ON_READ  = 0x10,
        CBLOCKED_ON_WRITE = 0x20
    } conn_blocked : 8;
#ifdef HAVE_OPENSSL
    /* SSL/TLS support */
    SSL_CTX *ssl_ctx;
    SSL *ssl_fd;
#endif
};

int ssl_setup(struct connection *conn, int sockfd, char *ssl_cert,
              char *ssl_key);

#endif /* TCPKALI_CONNECTION_H */
