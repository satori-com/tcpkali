/*
 * Copyright (c) 2014, 2015, 2016  Machine Zone, Inc.
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
#ifndef TCPKALI_SYSLIMITS_H
#define TCPKALI_SYSLIMITS_H

/*
 * Attempt to adjust system limits (open file counts) for high load.
 */
int adjust_system_limits_for_highload(int expected_sockets, int workers);

/*
 * Check that the system is prepared to handle high load by
 * observing sysctls, and the like.
 */
int check_system_limits_sanity(int expected_sockets, int workers);

/*
 * Check whether setsockopt() with a given option will give any effect.
 * RETURN VALUES:
 *  -1: We don't know whether setsockopt is effectful.
 *   0: The socket option is ineffectful.
 *   1: The socket option is likely effectful.
 * In case the socket option has no effect, a warning is printed.
 */
int check_setsockopt_effect(int so_option);

#endif /* TCPKALI_SYSLIMITS_H */
