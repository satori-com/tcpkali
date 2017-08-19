#define _GNU_SOURCE
#include <assert.h>

#include "tcpkali_common.h"
#include "tcpkali_connection.h"
#include "tcpkali_ssl.h"

int
ssl_setup(struct connection UNUSED *conn, int UNUSED sockfd,
          char UNUSED *ssl_cert, char UNUSED *ssl_key) {
#ifdef HAVE_OPENSSL
    conn->conn_blocked = 0;
    if(conn->ssl_ctx == NULL) {
        const SSL_METHOD *method = conn->conn_type == CONN_OUTGOING
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
                                       ? TLSv1_2_client_method()
                                       : TLSv1_2_server_method();
#else
                                       ? TLS_client_method()
                                       : TLS_server_method();
#endif
        if(method == NULL) {
            fprintf(stderr, "Can not create SSL method %lu\n", ERR_get_error());
            ERR_print_errors_fp(stderr);
            exit(1);
        }
        conn->ssl_ctx = SSL_CTX_new(method);
    }
    if(conn->ssl_ctx == NULL) {
        fprintf(stderr, "Can not create SSL context %lu\n", ERR_get_error());
        ERR_print_errors_fp(stderr);
        exit(1);
    } else {
        if(conn->conn_type == CONN_INCOMING) {
#ifdef  HAVE_SSL_CTX_SET_ECDH_AUTO
            SSL_CTX_set_ecdh_auto(conn->ssl_ctx, 1);
#endif
            if(SSL_CTX_use_certificate_file(conn->ssl_ctx, ssl_cert,
                                            SSL_FILETYPE_PEM)
               <= 0) {
                fprintf(stderr, "%s: %s\n", ssl_cert,
                        ERR_error_string(ERR_get_error(), NULL));
                exit(1);
            }
            if(SSL_CTX_use_PrivateKey_file(conn->ssl_ctx, ssl_key,
                                           SSL_FILETYPE_PEM)
               <= 0) {
                fprintf(stderr, "%s: %s\n", ssl_key,
                        ERR_error_string(ERR_get_error(), NULL));
                exit(1);
            }
        }
        if(!conn->ssl_fd) {
            conn->ssl_fd = SSL_new(conn->ssl_ctx);
            SSL_set_fd(conn->ssl_fd, sockfd);
            switch(conn->conn_type) {
            case CONN_OUTGOING:
                SSL_set_connect_state(conn->ssl_fd);
                break;
            case CONN_INCOMING:
                SSL_set_accept_state(conn->ssl_fd);
                break;
            case CONN_ACCEPTOR:
                assert(!"Unreachable");
                break;
            }
        }
        int status = -1;
        switch(conn->conn_type) {
        case CONN_OUTGOING:
            status = SSL_connect(conn->ssl_fd);
            break;
        case CONN_INCOMING:
            status = SSL_accept(conn->ssl_fd);
            break;
        case CONN_ACCEPTOR:
            assert(!"Unreachable");
            break;
        }
        switch(SSL_get_error(conn->ssl_fd, status)) {
        case SSL_ERROR_NONE:
            assert(status == 1);
            break;
        case SSL_ERROR_WANT_READ:
            assert(status == -1);
            conn->conn_blocked |= CBLOCKED_ON_READ;
            break;
        case SSL_ERROR_WANT_WRITE:
            assert(status == -1);
            conn->conn_blocked |= CBLOCKED_ON_WRITE;
            break;
        default:
            fprintf(stderr, "Can not create SSL connect %lu\n",
                    ERR_get_error());
            ERR_print_errors_fp(stderr);
            return 0;
        }
        if(status < 0) {
            conn->conn_blocked |= CBLOCKED_ON_INIT;
        }
    }
    assert(conn->ssl_fd != NULL);
#else
    assert(!"Unreachable");
#endif /* HAVE_OPENSSL */
    return 1;
}
