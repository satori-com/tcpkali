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
#ifndef TCPKALI_DATA_H
#define TCPKALI_DATA_H
#include <sys/types.h>

/*
 * Format data by escaping special characters. The buffer size should be
 * preallocated to at least (3 + 4*data_size).
 * If highlighting is enabled, another 32 bytes should be added.
 */
#define PRINTABLE_DATA_SUGGESTED_BUFFER_SIZE(size) (3 + 4 * (size) + 32)
char *printable_data(char *buffer, size_t buf_size, const void *data,
                     size_t data_size, int quote);
char *printable_data_highlight(char *buffer, size_t buf_size, const void *data,
                               size_t data_size, int quote,
                               size_t highlight_offset,
                               size_t highlight_length);

/*
 * Convert backslash-escaping back into corresponding bytes.
 * Supports octal (\0377), hexadecimal (\xHH) and usual \[nrfb] escape codes.
 * This function does in-place conversion.
 */
void unescape_data(void *data, size_t *initial_data_size);

/*
 * Read in the specified file contents, allocating memory and recording
 * the file size.
 * The caller is responsible for free()'ing the data pointer after successful
 * invocation.
 *
 * RETURN VALUES:
 *   0: file was read correctly,
 *  -1: some error was encountered.
 */
int read_in_file(const char *filename, char **data, size_t *size);

#endif /* TCPKALI_DATA_H */
