#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <StreamBoyerMooreHorspool.h>

int main() {
    size_t analyzed;
    struct StreamBMH_Occ occ;
    struct StreamBMH *ctx;
    SBMH_ALLOC_AND_INIT(ctx, &occ, "needle");
    assert(ctx);
    const unsigned char *needle = (const unsigned char *)"needle";
    const unsigned char *haystack = (const unsigned char *)"some needle data";
    size_t needle_size = strlen((char *)needle);
    size_t haystack_size = strlen((char *)haystack);

    assert(ctx->found == sbmh_false);

    analyzed = sbmh_feed(ctx, &occ, needle, needle_size, haystack, haystack_size);
    assert(analyzed == 11);
    assert(ctx->found == sbmh_true);

    analyzed = sbmh_feed(ctx, &occ, needle, needle_size, haystack, haystack_size);
    assert(analyzed == 0);
    assert(ctx->found == sbmh_true);

    sbmh_reset(ctx);

    analyzed = sbmh_feed(ctx, &occ, needle, needle_size, haystack, 8);
    assert(analyzed == 8);
    assert(ctx->found == sbmh_false);

    /* Feed the remainder of the haystack. */
    analyzed = sbmh_feed(ctx, &occ, needle, needle_size, haystack+8, haystack_size - 8);
    assert(analyzed == 3);
    assert(ctx->found == sbmh_true);

    free(ctx);
}
