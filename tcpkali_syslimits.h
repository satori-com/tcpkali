/*
    tcpkali: fast multi-core TCP load generator.

    Original author: Lev Walkin <lwalkin@machinezone.com>

    Copyright (C) 2014  Machine Zone, Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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

#endif  /* TCPKALI_SYSLIMITS_H */
