#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "hdr_histogram.h"

int main() {
    int measurements[] = {
        90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
        90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100
    };
    struct {
        int lowest;
        int highest;
    } trackable_values[] = {
        { 80, 110 },
        { 1, 1000 },
        { 1, 100000 }
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
            hdr_record_value(h, measurements[i]);
        }

        printf("\nPercentiles = %d/%d (50%%/90%%)\n",
            (int)hdr_value_at_percentile(h, 50.0),
            (int)hdr_value_at_percentile(h, 90.0)
        );
        hdr_percentiles_print(h, stdout, 2, 1, CLASSIC);
    }

    return 0;
}
