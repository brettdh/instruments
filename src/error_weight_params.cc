#include "error_weight_params.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include <deque>
using std::deque;

static const double MIN_SIZE_THRESHOLD = 0.01;

const size_t instruments::MAX_SAMPLES = 20;

// I want the weight on the oldest sample to be the minimum specified above.
// So, I solve for the weight on each new sample like this:
//    
//    MIN_SIZE_THRESHOLD = NEW_SAMPLE_WEIGHT ** MAX_SAMPLES
//    MIN_SIZE_THRESHOLD ** (1.0 / MAX_SAMPLES) = NEW_SAMPLE_WEIGHT
double instruments::NEW_SAMPLE_WEIGHT = pow(MIN_SIZE_THRESHOLD, 1.0/instruments::MAX_SAMPLES);

// new sample weight is large for agile, small for stable.
//  the 'gain' in EWMA parlance is always the weight on the old value.
//  so here's the gain for straight use in an EWMA calculation.
double instruments::EWMA_GAIN = 1.0 - NEW_SAMPLE_WEIGHT;

void instruments::update_ewma(double& ewma, double spot, double gain)
{
    ewma = ewma * gain + spot * (1.0 - gain);
}

double instruments::calculate_sample_weight(size_t sample_index, size_t num_samples)
{
    return pow(NEW_SAMPLE_WEIGHT, num_samples - sample_index);
}

double instruments::calculate_weighted_probability(size_t num_samples, double weight)
{
    return 1.0 / num_samples * weight;
}

static deque<double> normalizers;

static struct StaticIniter {
    StaticIniter() {
        normalizers.resize(instruments::MAX_SAMPLES + 1);
        normalizers[0] = strtod("NAN", NULL);
        for (size_t i = 1; i < normalizers.size(); ++i) {
            normalizers[i] = 0.0;
            for (size_t j = 0; j < i; ++j) {
                double weight = instruments::calculate_sample_weight(j, i);
                normalizers[i] += instruments::calculate_weighted_probability(i, weight);
            }
        }
    }
} initer;


double instruments::calculate_normalized_sample_weight(size_t sample_index, size_t num_samples)
{
    assert(num_samples > 0);
    assert(num_samples < normalizers.size());
    return calculate_sample_weight(sample_index, num_samples) / normalizers[num_samples];
}
