#include "tcpkali_ssl.h"
#include "tcpkali_common.h"
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#define MUTEX_TYPE       pthread_mutex_t
#define MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define THREAD_ID        pthread_self()

void
tcpkali_init_ssl() {
#ifdef HAVE_OPENSSL
#if OPENSSL_API_COMPAT < 0x10100000L
    SSL_library_init();
#else
    OPENSSL_init_ssl(0, NULL);
#endif /* OPENSSL_API_COMPAT */
    OpenSSL_add_all_algorithms();
#if OPENSSL_API_COMPAT < 0x10100000L
    SSL_load_error_strings();
    ERR_load_crypto_strings();
#endif /* OPENSSL_API_COMPAT */
#else
    assert(!"Unreachable");
#endif /* HAVE_OPENSSL */
}

#ifdef HAVE_OPENSSL
#if OPENSSL_API_COMPAT < 0x10100000L
static MUTEX_TYPE *mutex_buf= NULL;

static void UNUSED locking_function(int UNUSED mode, int UNUSED n, const char UNUSED *file, int UNUSED line) {
    if(mode & CRYPTO_LOCK) {
        MUTEX_LOCK(mutex_buf[n]);
    } else {
        MUTEX_UNLOCK(mutex_buf[n]);
    }
}

static unsigned long UNUSED id_function(void) {
  return ((unsigned long)THREAD_ID);
}
#endif /* OPENSSL_API_COMPAT */
#endif /* HAVE_OPENSSL */

int tcpkali_ssl_thread_setup(void) {
#ifdef HAVE_OPENSSL
#if OPENSSL_API_COMPAT < 0x10100000L
    int i;
    mutex_buf = malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
    if(!mutex_buf) {
        return 0;
    }
    for(i = 0;  i < CRYPTO_num_locks(); i++) {
        MUTEX_SETUP(mutex_buf[i]);
    }
    CRYPTO_set_id_callback(id_function);
    CRYPTO_set_locking_callback(locking_function);
#endif /* OPENSSL_API_COMPAT */
#endif /* HAVE_OPENSSL */
    return 1;
}

int tcpkali_ssl_thread_cleanup(void) {
#ifdef HAVE_OPENSSL
#if OPENSSL_API_COMPAT < 0x10100000L
    int i;
    if(!mutex_buf) {
        return 0;
    }
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    for(i = 0;  i < CRYPTO_num_locks(); i++) {
        MUTEX_CLEANUP(mutex_buf[i]);
    }
    free(mutex_buf);
    mutex_buf = NULL;
#endif /* OPENSSL_API_COMPAT */
#endif /* HAVE_OPENSSL */
    return 1;
}
