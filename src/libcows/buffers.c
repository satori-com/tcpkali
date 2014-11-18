/*
 * Copyright (c) 2013 Jacknyfe, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
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

#include <sys/types.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "libcows_common.h"
#include "buffers.h"

static cbuf *cbuf_new(size_t chunk_size);

struct cbuf_chain {
    STAILQ_HEAD(, cbuf) head;
    STAILQ_HEAD(, cbuf) zombies;
    ssize_t data_size;
    size_t chunk_size;
};

struct cbuf {
    STAILQ_ENTRY(cbuf) next;
    void *read_offset;
    void *write_offset;
    void *buffer_end;
    char *buf;
    void (*free)(void *_buf);
    char static_buf[0];
};

cbuf_chain *cbuf_chain_new(size_t chunk_size) {
    cbuf_chain *chain = calloc(1, sizeof(*chain));
    if(chain) {
            STAILQ_INIT(&chain->head);
            STAILQ_INIT(&chain->zombies);
            chain->chunk_size = chunk_size;
    }
    return chain;
}

void cbuf_chain_free(cbuf_chain *chain) {
    if(chain) {
        cbuf *cb, *tmp_cb;
        STAILQ_FOREACH_SAFE(cb, &chain->head, next, tmp_cb) {
            if(cb->free) cb->free(cb->buf);
            free(cb);
        }
        STAILQ_FOREACH_SAFE(cb, &chain->zombies, next, tmp_cb) {
            if(cb->free) cb->free(cb->buf);
            free(cb);
        }
        free(chain);
    }
}

size_t cbuf_chain_get_buffered_data_size(cbuf_chain *chain) {
    return chain ? chain->data_size : 0;
}

/*
 * Add the specified (buf) of size (size) at the end of the chain (chain).
 * Always returns 0.
 */
int cbuf_chain_add_bytes(const void *buf, size_t size, cbuf_chain *chain) {
    chain->data_size += size;
    cbuf *cb = STAILQ_LAST(&chain->head, cbuf, next);
    for (;;) {
        if(cb) {
            size_t space = cb->buffer_end - cb->write_offset;
            if(size <= space) {
                memcpy(cb->write_offset, buf, size);
                cb->write_offset += size;
                break;
            } else {
                memcpy(cb->write_offset , buf, space);
                cb->write_offset += space;
                size -= space;
                buf += space;
            }
        }
        cb = cbuf_new(chain->chunk_size);
        assert(cb);
        STAILQ_INSERT_TAIL(&chain->head, cb, next);
    }
    return 0;
}

void cbuf_chain_add_block(cbuf_chain *chain, void *buf, size_t size, void (*_free)(void *)) {
    cbuf *cb = cbuf_new(0);
    assert(cb);
    cb->buf = buf;
    cb->free = _free;
    cb->read_offset = buf;
    cb->write_offset = buf + size;
    cb->buffer_end = buf + size;
    STAILQ_INSERT_TAIL(&chain->head, cb, next);
    chain->data_size += size;
}

/*
 * Return a contiguous space into which to write. 
 */
void *cbuf_chain_get_write_chunk(cbuf_chain *chain, size_t *size, int guarantee) {
    cbuf *cb = STAILQ_LAST(&chain->head, cbuf, next);
    if(cb) {
        size_t space = cb->buffer_end - cb->write_offset;
        if(space >= guarantee) {
            *size = space;
            return cb->write_offset;
        } else if(cb->write_offset == cb->read_offset) {
            cb->write_offset = cb->read_offset = cb->buf;
            *size = cb->buffer_end - cb->write_offset;
            assert(*size >= guarantee);
            return cb->write_offset;
        }
    }
    cb = cbuf_new(chain->chunk_size);
    STAILQ_INSERT_TAIL(&chain->head, cb, next);
    *size = cb->buffer_end - cb->write_offset;
    assert(*size >= guarantee);
    return cb->write_offset;
}

/*
 * Advance the write pointer given by cbuf_chain_get_write_chunk().
 */
void cbuf_chain_advance_write_ptr(cbuf_chain *chain, size_t written) {
    chain->data_size += written;
    cbuf *cb = STAILQ_LAST(&chain->head, cbuf, next);
    assert(cb);
    assert(cb->buffer_end - cb->write_offset >= written);
    cb->write_offset += written;
}


/*
 * Get the first chunk of contiguous space.
 * Returns 0 if it there is no data in chain, and sets the (*chunk_size) to 0.
 */
void *cbuf_chain_get_read_chunk(cbuf_chain *chain, size_t *chunk_size) {
    cbuf *cb;
    for(cb = STAILQ_FIRST(&chain->head); cb; cb = STAILQ_FIRST(&chain->head)) {
        size_t avail = cb->write_offset - cb->read_offset;
        if(avail) {
            *chunk_size = avail;
            return cb->read_offset;
        } else {
            if(STAILQ_NEXT(cb, next)) {
                STAILQ_REMOVE_HEAD(&chain->head, next);
                if(cb->free) cb->free(cb->buf);
                free(cb);
            } else {
                break;
            }
        }
    }
    *chunk_size = 0;
    return 0;
}

/*
 * Remove the (consumed) number of bytes out of the head of the chain.
 */
void cbuf_chain_advance_read_ptr(cbuf_chain *chain, size_t consume) {
    chain->data_size -= consume;
    while(consume) {
        cbuf *cb = STAILQ_FIRST(&chain->head);
        size_t avail = cb->write_offset - cb->read_offset;
        if(avail >= consume) {
            cb->read_offset += consume;
            if(cb->read_offset == cb->write_offset)
                cb->read_offset = cb->write_offset = cb->buf;
            break;
        } else {
            assert(STAILQ_NEXT(cb, next));
            STAILQ_REMOVE_HEAD(&chain->head, next);
            consume -= avail;
            if(cb->free) cb->free(cb->buf);
            free(cb);
        }
    }
}

/*
 * Fill the I/O vector with the next pieces of from the chain.
 * This function can be invoked over and over again, yielding new data.
 * RETURN VALUE:
 *   0: No more data in the chain or no space in the given iov.
 *   n: Number of iov slots filled up with pointers to data.
 */
size_t cbuf_chain_fill_iov(cbuf_chain *chain, struct iovec *vec, size_t veclen, size_t *remaining) {
    size_t n = 0;

    if(remaining) *remaining = 0;

    if(!chain)
        return n;

    cbuf *cb, *tmp_cb;
    STAILQ_FOREACH_SAFE(cb, &chain->head, next, tmp_cb) {
        size_t avail = cb->write_offset - cb->read_offset;
        if(avail) {
            if(veclen) {
                vec[0].iov_base = cb->read_offset;
                vec[0].iov_len = avail;
                vec++;
                veclen--;
                n++;
                chain->data_size -= avail;
                STAILQ_REMOVE_HEAD(&chain->head, next);
                STAILQ_INSERT_HEAD(&chain->zombies, cb, next);
            } else {
                if(remaining) *remaining += avail;
                else break;
            }
        } else {
            STAILQ_REMOVE_HEAD(&chain->head, next);
            STAILQ_INSERT_HEAD(&chain->zombies, cb, next);
        }
    }

    return n;
}

/*
 * Collapse the chain into the single contiguous space.
 */
int cbuf_chain_flatten(cbuf_chain *chain) {
    cbuf *cb;
    size_t total_size = 0;

    // Figure out the total amount of data in the chain.
    STAILQ_FOREACH(cb, &chain->head, next) {
        total_size += cb->write_offset - cb->read_offset;
    }
    assert(chain->data_size == total_size);    // prepare to remove the above loop.

    cb = STAILQ_FIRST(&chain->head);
    if(!cb || total_size == cb->write_offset - cb->read_offset) {
        return 0;
    } else {
        cbuf *ncb = cbuf_new(total_size * 2);
        assert(ncb);
        cbuf *tmp_cb;
        STAILQ_FOREACH_SAFE(cb, &chain->head, next, tmp_cb) {
            size_t to_copy = cb->write_offset - cb->read_offset;
            memcpy(ncb->write_offset, cb->read_offset, to_copy);
            ncb->write_offset += to_copy;
            if(cb->free) cb->free(cb->buf);
            free(cb);
        }
        STAILQ_INIT(&chain->head);
        STAILQ_INSERT_HEAD(&chain->head, ncb, next);
        return 1;
    }
}

/*
 * Move (size) data from (from) chain to (to) chain, possibly with
 * zero-copy.
 */
void cbuf_chain_move(cbuf_chain *from, cbuf_chain *to, size_t size,
                     void (*map_cb)(void *, size_t, void *), void *key) {

    assert(from->data_size >= size);

    cbuf *cb, *tmp_cb;

    STAILQ_FOREACH_SAFE(cb, &from->head, next, tmp_cb) {
        size_t available = cb->write_offset - cb->read_offset;
        if(available <= size) {
            /* The whole block can be moved. */
            STAILQ_REMOVE_HEAD(&from->head, next);
            STAILQ_INSERT_TAIL(&to->head, cb, next);
            from->data_size -= available;
            to->data_size += available;
            if(map_cb) map_cb(cb->read_offset, available, key);
            size -= available;
        } else {
            /* Copy the content if the whole block cannot be extracted */
            size_t wr_space;
            while(size) {
                void *buf = cbuf_chain_get_write_chunk(to, &wr_space, 1);
                if(wr_space > size) wr_space = size;
                memcpy(buf, cb->read_offset, wr_space);
                if(map_cb) map_cb(buf, wr_space, key);
                cbuf_chain_advance_write_ptr(to, wr_space);
                cbuf_chain_advance_read_ptr(from, wr_space);
                size -= wr_space;
            }
            break;
        }
    }

    assert(size == 0);
}

/*
 * INTERNAL FUNCTIONS
 */

static cbuf *cbuf_new(size_t chunk_size) {
    cbuf *cb = malloc(sizeof(*cb) + chunk_size);
    if(cb) {
        cb->read_offset = cb->static_buf;
        cb->write_offset = cb->static_buf;
        cb->buffer_end = cb->static_buf + chunk_size;
        cb->buf = cb->static_buf;
        cb->free = 0;
    }
    return cb;
}

#ifdef UNIT_TEST_BUFFERS

int main() {
    int ret;
    size_t chunk_size;
    char *buf;

    printf("test 1\n");
    cbuf_chain *chain = cbuf_chain_new(5);
    assert(chain);

    printf("test 2\n");
    assert(cbuf_chain_get_read_chunk(chain, &chunk_size) == 0);
    assert(cbuf_chain_flatten(chain) == 0);
    assert(cbuf_chain_get_read_chunk(chain, &chunk_size) == 0);

    printf("test 3\n");
    ret = cbuf_chain_add_bytes(0, 0, chain);
    assert(ret == 0);
    assert(cbuf_chain_flatten(chain) == 0);
    assert(cbuf_chain_get_read_chunk(chain, &chunk_size) == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 0);

    printf("test 4\n");
    ret = cbuf_chain_add_bytes("abc", 3, chain);
    assert(ret == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 3);
    assert(cbuf_chain_get_read_chunk(chain, &chunk_size) != 0);
    assert(chunk_size == 3);
    assert(cbuf_chain_flatten(chain) == 0);
    assert(cbuf_chain_get_read_chunk(chain, &chunk_size) != 0);
    assert(chunk_size == 3);
    assert(cbuf_chain_get_buffered_data_size(chain) == 3);

    printf("test 5\n");
    cbuf_chain_advance_read_ptr(chain, 3);
    assert(cbuf_chain_flatten(chain) == 0);
    assert(cbuf_chain_get_read_chunk(chain, &chunk_size) == 0);
    assert(chunk_size == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 0);

    printf("test 6\n");
    ret = cbuf_chain_add_bytes("abcdef", 6, chain);
    assert(ret == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 6);
    buf = cbuf_chain_get_read_chunk(chain, &chunk_size);
    assert(chunk_size == 5);
    assert(memcmp(buf, "abcde", chunk_size) == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 6);

    assert(cbuf_chain_flatten(chain) == 1);
    buf = cbuf_chain_get_read_chunk(chain, &chunk_size);
    assert(chunk_size == 6);
    assert(memcmp(buf, "abcdef", chunk_size) == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 6);

    cbuf_chain_advance_read_ptr(chain, 2);
    assert(cbuf_chain_get_buffered_data_size(chain) == 4);
    buf = cbuf_chain_get_read_chunk(chain, &chunk_size);
    assert(chunk_size == 4);
    assert(memcmp(buf, "cdef", chunk_size) == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 4);

    ret = cbuf_chain_add_bytes("ghi", 3, chain);
    assert(cbuf_chain_get_buffered_data_size(chain) == 7);
    buf = cbuf_chain_get_read_chunk(chain, &chunk_size);
    assert(chunk_size == 7);
    assert(memcmp(buf, "cdefghi", chunk_size) == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 7);

    printf("test 7\n");
    chain = cbuf_chain_new(5);
    assert(chain);
    ret = cbuf_chain_add_bytes("abcdefghi", 9, chain);
    buf = cbuf_chain_get_read_chunk(chain, &chunk_size);
    assert(chunk_size == 5);
    assert(memcmp(buf, "abcde", chunk_size) == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 9);

    printf("test 8\n");
    cbuf_chain_advance_read_ptr(chain, 5);
    buf = cbuf_chain_get_read_chunk(chain, &chunk_size);
    assert(chunk_size == 4);
    assert(memcmp(buf, "fghi", chunk_size) == 0);
    assert(cbuf_chain_get_buffered_data_size(chain) == 4);

    printf("test 9\n");
    buf = cbuf_chain_get_write_chunk(chain, &chunk_size, 1);
    assert(chunk_size == 1);
    buf[0] = 'z';
    cbuf_chain_advance_write_ptr(chain, 1);
    buf = cbuf_chain_get_read_chunk(chain, &chunk_size);
    assert(chunk_size == 5);
    assert(memcmp(buf, "fghiz", chunk_size) == 0);
    buf = cbuf_chain_get_write_chunk(chain, &chunk_size, 1);
    assert(chunk_size == 5);
    assert(cbuf_chain_get_buffered_data_size(chain) == 5);

    return 0;
}

#endif  /* UNIT_TEST */
