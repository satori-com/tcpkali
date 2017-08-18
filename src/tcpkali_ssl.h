#ifndef TCPKALI_SSL_H
#define TCPKALI_SSL_H

#include "config.h"

#ifdef HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif /* HAVE_OPENSSL */

void tcpkali_init_ssl();
int tcpkali_ssl_thread_setup(void);
int tcpkali_ssl_thread_cleanup(void);

#endif /* TCPKALI_SSL_H */
