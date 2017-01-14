/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *     http://www.pcg-random.org
 */

/*
 * This file was mechanically generated from tests/check-pcg32.c
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include "pcg_basic.h"

/*
 * This code shows how you can cope if you're on a 32-bit platform (or a
 * 64-bit platform with a mediocre compiler) that doesn't support 128-bit math,
 * or if you're using the basic version of the library which only provides
 * 32-bit generation.
 *
 * Here we build a 64-bit generator by tying together two 32-bit generators.
 * Note that we can do this because we set up the generators so that each
 * 32-bit generator has a *totally different* different output sequence
 * -- if you tied together two identical generators, that wouldn't be nearly
 * as good.
 *
 * For simplicity, we keep the period fixed at 2^64.  The state space is
 * approximately 2^254 (actually  2^64 * 2^64 * 2^63 * (2^63 - 1)), which
 * is huge.
 */

typedef struct {
    pcg32_random_t gen[2];
} pcg32x2_random_t;

void pcg32x2_srandom_r(pcg32x2_random_t* rng, uint64_t seed1, uint64_t seed2,
                       uint64_t seq1,  uint64_t seq2)
{
    uint64_t mask = ~0ull >> 1;
    // The stream for each of the two generators *must* be distinct
    if ((seq1 & mask) == (seq2 & mask)) 
        seq2 = ~seq2;
    pcg32_srandom_r(rng->gen,   seed1, seq1);
    pcg32_srandom_r(rng->gen+1, seed2, seq2);
}

uint64_t pcg32x2_random_r(pcg32x2_random_t* rng)
{
    return ((uint64_t)(pcg32_random_r(rng->gen)) << 32)
           | pcg32_random_r(rng->gen+1);
}

/* See other definitons of ..._boundedrand_r for an explanation of this code. */

uint64_t pcg32x2_boundedrand_r(pcg32x2_random_t* rng, uint64_t bound)
{
    uint64_t threshold = -bound % bound;
    for (;;) {
        uint64_t r = pcg32x2_random_r(rng);
        if (r >= threshold)
            return r % bound;
    }
}

int dummy_global;

int main(int argc, char** argv)
{
    // Read command-line options

    int rounds = 5;
    bool nondeterministic_seed = false;
    int round, i;

    ++argv;
    --argc;
    if (argc > 0 && strcmp(argv[0], "-r") == 0) {
        nondeterministic_seed = true;
        ++argv;
        --argc;
    }
    if (argc > 0) {
        rounds = atoi(argv[0]);
    }

    // In this version of the code, we'll use our custom rng rather than
    // one of the provided ones.

    pcg32x2_random_t rng;

    // You should *always* seed the RNG.  The usual time to do it is the
    // point in time when you create RNG (typically at the beginning of the
    // program).
    //
    // pcg32x2_srandom_r takes four 64-bit constants (the initial state, and 
    // the rng sequence selector; rngs with different sequence selectors will
    // *never* have random sequences that coincide, at all) - the code below
    // shows three possible ways to do so.

    if (nondeterministic_seed) {
        // Seed with external entropy -- the time and some program addresses
        // (which will actually be somewhat random on most modern systems).
        // A better solution, entropy_getbytes, using /dev/random, is provided
        // in the full library.
        
        pcg32x2_srandom_r(&rng, time(NULL) ^ (intptr_t)&printf,
                               ~time(NULL) ^ (intptr_t)&pcg32_random_r,
                                (intptr_t)&rounds,
                                (intptr_t)&dummy_global);
    } else {
        // Seed with a fixed constant

        pcg32x2_srandom_r(&rng, 42u, 42u, 54u, 54u);
    }

    printf("pcg32x2_random_r:\n"
           "      -  result:      64-bit unsigned int (uint64_t)\n"
           "      -  period:      2^64   (* ~2^126 streams)\n"
           "      -  state space: ~2^254\n"
           "      -  state type:  pcg32x2_random_t (%zu bytes)\n"
           "      -  output func: XSH-RR (x 2)\n"
           "\n",
           sizeof(pcg32x2_random_t));

    for (round = 1; round <= rounds; ++round) {
        printf("Round %d:\n", round);
        /* Make some 32-bit numbers */
        printf("  64bit:");
        for (i = 0; i < 6; ++i) {
            if (i > 0 && i % 3 == 0)
                printf("\n\t");
            printf(" 0x%016llx", pcg32x2_random_r(&rng));
        }
        printf("\n");

        /* Toss some coins */
        printf("  Coins: ");
        for (i = 0; i < 65; ++i)
            printf("%c", pcg32x2_boundedrand_r(&rng, 2) ? 'H' : 'T');
        printf("\n");

        /* Roll some dice */
        printf("  Rolls:");
        for (i = 0; i < 33; ++i) {
            printf(" %d", (int)pcg32x2_boundedrand_r(&rng, 6) + 1);
        }
        printf("\n");

        /* Deal some cards */
        enum { SUITS = 4, NUMBERS = 13, CARDS = 52 };
        char cards[CARDS];

        for (i = 0; i < CARDS; ++i)
            cards[i] = i;

        for (i = CARDS; i > 1; --i) {
            int chosen = pcg32x2_boundedrand_r(&rng, i);
            char card = cards[chosen];
            cards[chosen] = cards[i - 1];
            cards[i - 1] = card;
        }

        printf("  Cards:");
        static const char number[] = {'A', '2', '3', '4', '5', '6', '7',
                                      '8', '9', 'T', 'J', 'Q', 'K'};
        static const char suit[] = {'h', 'c', 'd', 's'};
        for (i = 0; i < CARDS; ++i) {
            printf(" %c%c", number[cards[i] / SUITS], suit[cards[i] % SUITS]);
            if ((i + 1) % 22 == 0)
                printf("\n\t");
        }
        printf("\n");

        printf("\n");
    }

    return 0;
}
