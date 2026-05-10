#ifndef __FILTER_H__
#define __FILTER_H__

#include <stdint.h>

/**
 * LPF (Low-Pass Filter) Configuration Structure
 * Configuration parameters for first-order exponential low-pass filter
 * Each data field can have its own independent configuration
 */
typedef struct {
    uint16_t cutoff_freq_hz;   /* Cutoff frequency in Hz */
    uint16_t sample_rate_hz;   /* Sampling frequency in Hz */
} LpfConfig;

/**
 * Low-Pass Filter (LPF) - First-order exponential filter
 * Using fixed-point arithmetic for integer values
 */

typedef struct {
    int32_t prev_output;
    int16_t alpha_fixed;  /* alpha in fixed-point (0-32768 represents 0-1.0) */
} LpfState;

/**
 * LPF Filter Handle (opaque structure)
 */
struct LpfFilter {
    LpfState state;        /* Single filter state for one data stream */
    LpfConfig config;      /* Configuration parameters */
};
typedef struct LpfFilter LpfFilter;

/**
 * Create a new LPF filter instance
 * 
 * @param config: Configuration parameters (cutoff_freq_hz, sample_rate_hz, num_filters)
 * @return: Pointer to LpfFilter structure, NULL on failure
 */
int lpf_create(const LpfConfig *config, LpfFilter *filter);

/**
 * Apply LPF to a single value
 * Each filter instance processes one data stream independently
 * 
 * @param filter: Pointer to LpfFilter structure
 * @param input_value: Input integer value to filter
 * @return: Filtered output value
 */
int32_t lpf_apply(LpfFilter *filter, int32_t input_value);

/**
 * Reset filter to initial state
 * 
 * @param filter: Pointer to LpfFilter structure
 */
void lpf_reset(LpfFilter *filter);

/**
 * Initialize filter with an initial value
 * 
 * @param filter: Pointer to LpfFilter structure
 * @param initial_value: Initial value for the filter state
 */
void lpf_init_value(LpfFilter *filter, int32_t initial_value);

/**
 * Get filter configuration
 * 
 * @param filter: Pointer to LpfFilter structure
 * @return: Pointer to filter configuration
 */
const LpfConfig* lpf_get_config(LpfFilter *filter);

/**
 * Destroy LPF filter instance and free resources
 * 
 * @param filter: Pointer to LpfFilter structure
 */
void lpf_destroy(LpfFilter *filter);

#endif /* __FILTER_H__ */
