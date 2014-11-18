#ifndef LIBCOWS_BASE64_H
#define LIBCOWS_BASE64_H

#include <sys/types.h>

/*
 * Encode the given string using the base64 encoder.
 * The resulting encoded size will be placed into *out_b64buffer_size.
 * The function will return 0 if buffers are not sized properly.
 */
char *libcows_base64_encode(
        const void *data, size_t data_size,
        void *out_b64bufer, size_t *out_b64buffer_size);

#endif	/* LIBCOWS_BASE64_H */
