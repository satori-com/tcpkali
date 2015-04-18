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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tcpkali_data.h"

/*
 * Format data by escaping special characters. The buffer size should be
 * preallocated to at least (4*data_size+3).
 */
char *printable_data(char *buffer, size_t buf_size, const void *data, size_t data_size, int quote) {
    const unsigned char *p = data;
    const unsigned char *pend = p + data_size;

    if(buf_size < PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(data_size))
        return NULL;

    char *b = buffer;
    if(quote) *b++ = '"';

    for(; p < pend; p++) {
        switch(*p) {
        case '\r': *b++ = '\\'; *b++ = 'r'; break;
        case '\n': *b++ = '\\'; *b++ = 'n';
            if(p+1 == pend) break;
            *b++ = '\n'; *b++ = '\t'; break;
        case 32 ... 33: *b++ = *p; break;
        case 35 ... 126: *b++ = *p; break;
        case 34:    /* '"' */
            if(quote) *b++ = '\\';
            *b++ = '"'; break;
        default:
            b += snprintf(b, buf_size - (b - buffer), "\\%03o", *p);
            break;
        }
    }

    if(quote) *b++ = '"';
    *b++ = '\0';
    assert((size_t)(b - buffer) <= buf_size);
    return buffer;
}
