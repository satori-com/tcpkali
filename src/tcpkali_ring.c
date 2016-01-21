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
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "tcpkali_ring.h"

struct ring_buffer *
ring_buffer_new(size_t unit_size) {
    struct ring_buffer *rb = calloc(1, sizeof(*rb));
    ring_buffer_init(rb, unit_size);
    return rb;
}

void
ring_buffer_init(struct ring_buffer *rb, size_t unit_size) {
    rb->unit_size = unit_size;
    rb->size = rb->unit_size * 16;
    rb->ptr = malloc(rb->size);
    rb->left = rb->right = rb->ptr;
    assert(rb->ptr);
}

void
ring_buffer_grow(struct ring_buffer *rb) {
    assert(rb->unit_size);
    assert(rb->size);

    assert(rb->ptr <= rb->left);
    assert(rb->ptr <= rb->right);
    assert(rb->right <= rb->ptr + rb->size);
    assert(rb->left <= rb->ptr + rb->size);

    size_t new_size = 2 * rb->size;
    void *ptr = malloc(new_size);
    assert(ptr);

    if(rb->left <= rb->right) {
        memcpy(ptr, rb->left, rb->right - rb->left);
        free(rb->ptr);
        rb->ptr = ptr;
        rb->right = ptr + (rb->right - rb->left);
        rb->left = rb->ptr;
        rb->size = new_size;
    } else {
        size_t first_block_size = rb->size - (rb->left - rb->ptr);
        memcpy(ptr, rb->left, first_block_size);
        size_t second_block_size = rb->right - rb->ptr;
        memcpy((char *)ptr + first_block_size, rb->ptr, second_block_size);
        free(rb->ptr);
        rb->ptr = ptr;
        rb->right = ptr + first_block_size + second_block_size;
        rb->left = rb->ptr;
        rb->size = new_size;
    }
}

#ifdef TCPKALI_RING_UNIT_TEST

static void
dump(struct ring_buffer *rb) {
    int i = 0;
    void *p;
    assert(rb->unit_size == sizeof(int));
    printf("Dumping size %d units of size %d\n",
           (int)(rb->size / rb->unit_size), (int)rb->unit_size);
    for(p = rb->ptr; (size_t)(p - rb->ptr) < rb->size;
        p = (char *)p + rb->unit_size) {
        if(p == rb->left && p == rb->right)
            printf("[%03d] %d <- left & right\n", i, *(int *)p);
        else if(p == rb->left)
            printf("[%03d] %d <- left\n", i, *(int *)p);
        else if(p == rb->right)
            printf("[%03d] %d <- right\n", i, *(int *)p);
        else
            printf("[%03d] %d\n", i, *(int *)p);
        i++;
    }
}

int
main() {
    struct ring_buffer *rb = ring_buffer_new(sizeof(int));
    int iterations = 1000;
    int next_add = 1;
    int next_remove = 1;
    int added = 0;
    int removed = 0;
    int got;
    int n = -1;

    got = ring_buffer_get(rb, &n);
    assert(got == 0);
    ring_buffer_add(rb, 0);
    got = ring_buffer_get(rb, &n);
    assert(got == 1);
    assert(n == 0);

    /*
     * Add a few measurements and remove a few.
     */
    while(iterations--) {
        int to_add = random() % 10;
        int to_remove = random() % 10;
        while(to_add--) {
            ring_buffer_add(rb, next_add);
            next_add++;
            added++;
        }
        while(to_remove--) {
            int n;
            if(!ring_buffer_get(rb, &n)) break;
            if(n != next_remove) {
                dump(rb);
            }
            assert(n == next_remove);
            next_remove++;
            removed++;
        }
    }

    assert(removed <= added);

    /*
     * Remove the rest.
     */
    for(; removed < added; removed++) {
        got = ring_buffer_get(rb, &n);
        assert(got);
        assert(n == next_remove);
        next_remove++;
    }

    assert(rb->unit_size == sizeof(int));

    return 0;
}

#endif /* TCPKALI_RING_UNIT_TEST */
