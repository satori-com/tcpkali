#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "hdr_histogram.h"

/*
 * Regression test against
 * https://github.com/HdrHistogram/HdrHistogram_c/issues/6
 */
static void check_out_of_range() {
    struct hdr_histogram *h;
    int ret = hdr_init(1, 1000, 4, &h);
    assert(ret == 0);
    assert(h);

    enum {
        EXPECT_TRUE,
        EXPECT_FALSE,
    } state = EXPECT_TRUE;
    for(int i = 0; i < 50000; i++) {
        bool b = hdr_record_value(h, i);
        if(state == EXPECT_TRUE) {
            state = (b == true) ? EXPECT_TRUE : EXPECT_FALSE;
        } else {
            assert(b == false);
        }
    }
}

int main() {
    int measurements[] = {
        90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
        90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100
    };
    struct {
        int lowest;
        int highest;
    } trackable_values[] = {
        { 1, 100000 },
        { 1, 1000 },
        { 1, 100 }
    };

    for(size_t tv = 0; tv < sizeof(trackable_values)/sizeof(trackable_values[0]); tv++) {
        struct hdr_histogram *h = 0;
        printf("=== Test %d: hdr_init(%d, %d)\n", (int)tv+1,
                trackable_values[tv].lowest, trackable_values[tv].highest);

        /* Initialize histogram library */
        int ret = hdr_init(trackable_values[tv].lowest,
                           trackable_values[tv].highest, 5, &h);
        assert(ret == 0);
        assert(h);

        /* Fill with values. */
        printf("Values:");
        for(size_t i=0; i < sizeof(measurements)/sizeof(measurements[0]); i++) {
            printf(" %d", measurements[i]);
            bool b = hdr_record_value(h, measurements[i]);
            assert(b == true);
        }

        printf("\nPercentiles = %d/%d/%d (50%%/90%%/99%%)\n",
            (int)hdr_value_at_percentile(h, 50.0),
            (int)hdr_value_at_percentile(h, 90.0),
            (int)hdr_value_at_percentile(h, 99.0)
        );
        hdr_percentiles_print(h, stdout, 2, 1, CLASSIC);

        assert(hdr_value_at_percentile(h, 50.0) == 95);
        assert(hdr_value_at_percentile(h, 90.0) == 99);
        assert(hdr_value_at_percentile(h, 99.0) == 100);
    }

    check_out_of_range();

    return 0;
}
