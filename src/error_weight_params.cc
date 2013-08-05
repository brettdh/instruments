#include "error_weight_params.h"

#include <math.h>

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
