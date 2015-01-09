/**
 * hdr_dbl_histogram.h
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef HDR_DBL_HISTOGRAM_H
#define HDR_DBL_HISTOGRAM_H 1

#include <stdint.h>
#include "hdr_histogram.h"

struct hdr_dbl_histogram
{
    double current_lowest_value;
    double current_highest_value;
    int64_t highest_to_lowest_value_ratio;
    double int_to_dbl_conversion_ratio;
    double dbl_to_int_conversion_ratio;

    struct hdr_histogram values;
};

int hdr_dbl_init(
    int64_t highest_to_lowest_value_ratio,
    int32_t significant_figures,
    struct hdr_dbl_histogram** result);

bool hdr_dbl_record_value(struct hdr_dbl_histogram* h, double value);
bool hdr_dbl_record_corrected_value(struct hdr_dbl_histogram* h, double value, double expected_interval);
/**
 * Add the values from the addend histogram to the sum histogram.
 */
int64_t hdr_dbl_add(struct hdr_dbl_histogram* sum, struct hdr_dbl_histogram* addend);
void hdr_dbl_reset(struct hdr_dbl_histogram* h);

int64_t hdr_dbl_count_at_value(struct hdr_dbl_histogram* h, double value);

#endif