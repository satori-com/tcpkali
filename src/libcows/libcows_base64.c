/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
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
 *
 */

#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <stdio.h>	/* for NULL */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

static const unsigned char _sf_uc_ib[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/==";

/*
 * Encode the given data using Base64 encoding.
 */
char *
libcows_base64_encode(const void *data, size_t data_size, void *out_buf, size_t *out_size) {
	char *ou;
	const unsigned char *p = (const unsigned char *)data;
	const void *data_end = (const char *)data + data_size;
	size_t estimated_output_size;
	int nc = 0;

	if(data == NULL) {
		errno = EINVAL;
		return NULL;
	}

	estimated_output_size = + (4 * (data_size + 2) / 3) + 1;
	estimated_output_size += estimated_output_size / 76;	/* For \n's */
    if(!out_size || *out_size < estimated_output_size)
        return 0;

	ou = out_buf;

	while((char *)data_end - (char *)p >= 3) {
		*ou = _sf_uc_ib[ *p >> 2 ];
		ou[1] = _sf_uc_ib[ ((*p & 0x03) << 4) | (p[1] >> 4) ];
		ou[2] = _sf_uc_ib[ ((p[1] & 0x0F) << 2) | (p[2] >> 6) ];
		ou[3] = _sf_uc_ib[ p[2] & 0x3F ];

		p+=3;
		ou+=4;

		nc+=4;
		if((nc % 76) == 0) *ou++='\n';
	}
	if((char *)data_end - (char *)p == 2) {
		*ou++ = _sf_uc_ib[ *p >> 2 ];
		*ou++ = _sf_uc_ib[ ((*p & 0x03) << 4) | (p[1] >> 4) ];
		*ou++ = _sf_uc_ib[ ((p[1] & 0x0F) << 2) ];
		*ou++ = '=';
	} else if((char *)data_end - (char *)p == 1) {
		*ou++ = _sf_uc_ib[ *p >> 2 ];
		*ou++ = _sf_uc_ib[ ((*p & 0x03) << 4) ];
		*ou++ = '=';
		*ou++ = '=';
	}

	*ou = '\0';

	assert(*out_size > ((void *)ou - out_buf));
	*out_size = ((void *)ou - out_buf);

	return out_buf;
}
