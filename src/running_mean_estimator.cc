#include <assert.h>
#include "running_mean_estimator.h"

double
RunningMeanEstimator::getEstimate()
{
    // TODO: provide initial value?
    assert(count > 0);
    return mean;
}

RunningMeanEstimator::RunningMeanEstimator()
    : mean(0.0), count(0)
{
}

void
RunningMeanEstimator::storeNewObservation(double value)
{
    if (count == 0) {
        mean = value;
    } else {
        mean = ((mean * count) + value) / (count + 1);
    }
    count++;
}
