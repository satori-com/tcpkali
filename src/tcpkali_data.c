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
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "tcpkali_terminfo.h"
#include "tcpkali_data.h"

/*
 * Format data by escaping special characters. The buffer size should be
 * preallocated to at least (4*data_size+3).
 */
char *
printable_data(char *buffer, size_t buf_size, const void *data,
               size_t data_size, int quote) {
    return printable_data_highlight(buffer, buf_size, data, data_size, quote, 0, 0);
}

char *
printable_data_highlight(char *buffer, size_t buf_size, const void *data,
                         size_t data_size, int quote, size_t highlight_offset,
                         size_t highlight_length) {
    const unsigned char *p = data;
    const unsigned char *pend = p + data_size;

    if(buf_size < PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(data_size)) return NULL;

    char *b = buffer;
    if(quote) *b++ = '"';

    const unsigned char *hl_start = NULL;
    const unsigned char *hl_pre_end = NULL;
    if(highlight_length > 0 && highlight_offset < data_size) {
        hl_start = data + highlight_offset;
        hl_pre_end = hl_start + highlight_length - 1;
        if(hl_pre_end >= pend) {
            hl_pre_end = pend - 1;
            assert(hl_start < pend);
        }
    }

    for(; p < pend; p++) {
        if(hl_start == p) {
            b += snprintf(b, buf_size - (b - buffer), "%s",
                          tk_attr(TKA_HIGHLIGHT));
        }


        switch(*p) {
        case '\r':
            *b++ = '\\';
            *b++ = 'r';
            break;
        case '\n':
            *b++ = '\\';
            *b++ = 'n';
            if(p + 1 == pend) break;
            *b++ = '\n';
            *b++ = '\t';
            break;
        case 32 ... 33:
            *b++ = *p;
            break;
        case '\\': /* ascii 92 */
            *b++ = '\\';
            *b++ = '\\';
            break;
        case 35 ... 91:
        case 93 ... 126:
            *b++ = *p;
            break;
        case 34: /* '"' */
            if(quote) *b++ = '\\';
            *b++ = '"';
            break;
        default:
            b += snprintf(b, buf_size - (b - buffer), "\\x%02x", *p);
            break;
        }

        if(hl_pre_end == p) {
            b += snprintf(b, buf_size - (b - buffer), "%s", tk_attr(TKA_NORMAL));
        }
    }

    if(quote) *b++ = '"';
    *b++ = '\0';
    assert((size_t)(b - buffer) <= buf_size);
    return buffer;
}

void
unescape_data(void *data, size_t *initial_data_size) {
    char *r = data;
    char *w = data;

    /* Avoid unescaping non-existing string. */
    if(data == NULL || (initial_data_size && !*initial_data_size)) return;

    size_t data_size = initial_data_size ? *initial_data_size : strlen(data);
    char *end = data + data_size;

    for(; r < end; r++, w++) {
        switch(*r) {
        default:
            *w = *r;
            break;
        case '\\':
            r++;
            if(r == end) {
                *w = '\\';
                break;
            }
            switch(*r) {
            case 'n':
                *w = '\n';
                break;
            case 'r':
                *w = '\r';
                break;
            case 'f':
                *w = '\f';
                break;
            case 'b':
                *w = '\b';
                break;
            case 'x': {
                /* Do not parse more than 2 symbols (ff) */
                char digits[3];
                char *endptr = (r + 3) < end ? (r + 3) : end;
                memcpy(digits, r + 1, endptr - r - 1); /* Ignore leading 'x' */
                digits[2] = '\0';
                char *digits_end = digits;
                unsigned long l = strtoul(digits, &digits_end, 16);
                if(digits_end == digits) {
                    *w++ = '\\';
                    *w = *r;
                } else {
                    r += (digits_end - digits);
                    *w = (l & 0xff);
                }
            } break;
            case '0': {
                char digits[5];
                char *endptr = (r + 4) < end ? (r + 4) : end;
                memcpy(digits, r, endptr - r);
                digits[4] = '\0';
                char *digits_end = digits;
                unsigned long l = strtoul(digits, &digits_end, 8);
                if(digits_end == digits) {
                    *w = '\0';
                } else {
                    r += (digits_end - digits) - 1;
                    *w = (l & 0xff);
                }
            } break;
            case '\\':
                *w++ = '\\';
                *w = *r;
                break;
            default:
                *w = *r;
                break;
            }
        }
    }
    *w = '\0';

    if(initial_data_size) *initial_data_size = (w - (char *)data);
}

int
read_in_file(const char *filename, char **data, size_t *size) {
    FILE *fp = fopen(filename, "rb");
    if(!fp) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long off = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(!off) {
        fprintf(stderr, "%s: Warning: file has no content\n", filename);
    }

    *data = malloc(off + 1);
    if(*data == NULL) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        return -1;
    }
    size_t r = fread(*data, 1, off, fp);
    assert((long)r == off);
    (*data)[off] = '\0'; /* Just in case. */
    *size = off;

    fclose(fp);

    return 0;
}
