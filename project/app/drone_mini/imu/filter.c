#include "filter.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/********************** LOW-PASS FILTER ******************/
#define PI (3.14159265f)
#define FIXED_POINT_SCALE (32768) /* Scale factor for fixed-point representation (0-32768 represents 0-1.0) */

/**
 * Create a new LPF filter instance
 * 
 * @param config: Configuration parameters (cutoff_freq_hz, sample_rate_hz, num_filters)
 * @return: Pointer to LpfFilter structure, NULL on failure
 */
int lpf_create(const LpfConfig *config, LpfFilter *filter)
{
    float alpha;
    float two_pi_fc;
    float denominator;

    if (config == NULL || config->sample_rate_hz <= 0 || config->cutoff_freq_hz <= 0) {
        return -1;
    }

    if (config->cutoff_freq_hz >= config->sample_rate_hz) {
        return -1;
    }

    memcpy(&filter->config, config, sizeof(LpfConfig));

    /* Calculate alpha in fixed-point format (0-32768 represents 0-1.0) */
    two_pi_fc = 2.0f * PI * config->cutoff_freq_hz;
    denominator = (0 - two_pi_fc / (float)config->sample_rate_hz);
    alpha = 1 - expf(denominator); 
    // denominator = two_pi_fc + (float)config->sample_rate_hz; /* Using the standard RC filter formula */
    // alpha = two_pi_fc / denominator;

    printf("Calculated alpha for cutoff_freq_hz=%d Hz, sample_rate_hz=%d Hz: alpha=%.9f\n",
           config->cutoff_freq_hz, config->sample_rate_hz, alpha);

    /* Convert alpha to fixed-point: multiply by 32768 */
    int16_t alpha_fixed = (int16_t)(alpha * FIXED_POINT_SCALE);
    if (alpha_fixed < 0) {
        alpha_fixed = 0;
    }
    if (alpha_fixed > FIXED_POINT_SCALE) {
        alpha_fixed = FIXED_POINT_SCALE;
    }

    /* Initialize filter state */
    filter->state.prev_output = 0;
    filter->state.alpha_fixed = alpha_fixed;

    return 1;
}

/**
 * Apply LPF to a single value
 * 
 * @param filter: Pointer to LpfFilter structure
 * @param filter_index: Index of the filter to use (0 to num_filters-1)
 * @param input_value: Input integer value to filter
 * @return: Filtered output value
 */
int32_t lpf_apply(LpfFilter *filter, int32_t input_value)
{
    int32_t output;
    int16_t alpha_fp;
    int32_t one_minus_alpha_fp;

    if (filter == NULL) {
        return input_value;
    }

    alpha_fp = filter->state.alpha_fixed;
    one_minus_alpha_fp = FIXED_POINT_SCALE - alpha_fp;

    /* Fixed-point calculation: y = alpha*x + (1-alpha)*y_prev */
    output = (int32_t)(((long long int)alpha_fp * input_value) / FIXED_POINT_SCALE);
    output += (int32_t)(((long long int)one_minus_alpha_fp * filter->state.prev_output) / FIXED_POINT_SCALE);

    filter->state.prev_output = output;

    return output;
}

/**
 * Reset a specific filter to initial state
 * 
 * @param filter: Pointer to LpfFilter structure
 * @param filter_index: Index of the filter to reset
 */
void lpf_reset(LpfFilter *filter)
{
    if (filter == NULL) {
        return;
    }

    filter->state.prev_output = 0;
}

/**
 * Initialize a filter with an initial value
 * 
 * @param filter: Pointer to LpfFilter structure
 * @param filter_index: Index of the filter to initialize
 * @param initial_value: Initial value for the filter state
 */
void lpf_init_value(LpfFilter *filter, int32_t initial_value)
{
    if (filter == NULL) {
        return;
    }

    filter->state.prev_output = initial_value;
}

/**
 * Get filter configuration
 * 
 * @param filter: Pointer to LpfFilter structure
 * @return: Pointer to filter configuration
 */
const LpfConfig* lpf_get_config(LpfFilter *filter)
{
    if (filter == NULL) {
        return NULL;
    }

    return &filter->config;
}

/**
 * Destroy LPF filter instance and free resources
 * 
 * @param filter: Pointer to LpfFilter structure
 */
void lpf_destroy(LpfFilter *filter)
{
    if (filter == NULL) {
        return;
    }

    free(filter);
}
/* End of Low-Pass Filter */


    