/*
 * Copyright (c) 2015  Machine Zone, Inc.
 *
 * Original author: Lev Walkin <lwalkin@machinezone.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef TCPKALI_RING_H
#define TCPKALI_RING_H

struct ring_buffer {
    void *ptr;
    void *left;
    void *right;
    size_t size;      /* Buffer size, bytes */
    size_t unit_size; /* Single unit size, bytes */
};

struct ring_buffer *ring_buffer_new(size_t unit_size);
void ring_buffer_init(struct ring_buffer *, size_t unit_size);

#define ring_buffer_free(rb) \
    do {                     \
        if(rb) {             \
            free(rb->ptr);   \
            free(rb);        \
        }                    \
    } while(0)

/*
 * Add a specified element to the ring.
 * Returns non-zero value if the ring has grown because of it.
 */
#define ring_buffer_add(rb, datum)             \
    ({                                         \
        typeof(datum) d = datum;               \
        void *np = ring_buffer_next_right(rb); \
        int grown = 0;                         \
        if(!np) {                              \
            ring_buffer_grow(rb);              \
            grown = 1;                         \
            np = ring_buffer_next_right(rb);   \
            assert(np);                        \
        }                                      \
        assert(rb->unit_size == sizeof(d));    \
        typeof(d) *p = rb->right;              \
        rb->right = np;                        \
        *p = d;                                \
        grown;                                 \
    })

#define ring_buffer_get(rb, datump)                             \
    ({                                                          \
        typeof(datump) p = rb->ptr;                             \
        typeof(datump) l = rb->left;                            \
        typeof(datump) r = rb->right;                           \
        assert(rb->unit_size == sizeof(*p));                    \
        int got;                                                \
        if(l < r) {                                             \
            *(datump) = *l;                                     \
            rb->left = ++l;                                     \
            got = 1;                                            \
        } else if(l == r) {                                     \
            got = 0;                                            \
        } else {                                                \
            if((((char *)l - (char *)p)) < (ssize_t)rb->size) { \
                *(datump) = *l;                                 \
                rb->left = ++l;                                 \
                got = 1;                                        \
            } else {                                            \
                l = p;                                          \
                if(l < r) {                                     \
                    *(datump) = *l;                             \
                    rb->left = ++l;                             \
                    got = 1;                                    \
                } else {                                        \
                    got = 0;                                    \
                }                                               \
            }                                                   \
        }                                                       \
        got;                                                    \
    })

static inline void *__attribute__((unused))
ring_buffer_next_right(struct ring_buffer *rb) {
    char *p = rb->ptr, *l = rb->left, *r = rb->right;
    if(r < l) {
        r += rb->unit_size;
        if(r >= l)
            return 0;
        else
            return r;
    } else {
        if((r - p) < (ssize_t)(rb->size - rb->unit_size)) {
            r += rb->unit_size;
            assert((r - p) <= (ssize_t)rb->size);
            return r;
        } else {
            if((l - p) <= (ssize_t)rb->unit_size)
                return 0;
            else
                return p;
        }
    }
}


void ring_buffer_grow(struct ring_buffer *);

#endif /* TCPKALI_RING_H */
