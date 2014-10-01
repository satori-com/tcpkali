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
#ifndef TCPKALI_ATOMIC_H
#define TCPKALI_ATOMIC_H

typedef uint32_t atomic_t;
typedef uint64_t atomic64_t;

static inline void __attribute__((unused)) atomic_increment(atomic_t *i) {
    asm volatile("lock incl %0" : "+m" (*i));
}

static inline void __attribute__((unused)) atomic_decrement(atomic_t *i) {
    asm volatile("lock decl %0" : "+m" (*i));
}

static inline void __attribute__((unused)) atomic_add(atomic64_t *i, uint64_t v) {
    asm volatile("lock addq %1, %0" : "+m" (*i) : "r" (v));
}

#endif  /* TCPKALI_ATOMIC_H */
