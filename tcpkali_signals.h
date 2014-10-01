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
#ifndef TCPKALI_SIGNAL_H
#define TCPKALI_SIGNAL_H

/*
 * Protect this thread from receiving term (SIGINT) signals.
 */
void block_term_signals();

/*
 * Unblock term signals (SIGINT) and handle it by setting a flag.
 */
void flagify_term_signals(int *flag);


#endif  /* TCPKALI_SIGNAL_H */
