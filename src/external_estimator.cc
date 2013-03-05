#include "external_estimator.h"

ExternalEstimator::ExternalEstimator(const std::string& name)
    : Estimator(name), lastValue(0.0)
{
}

void ExternalEstimator::addObservation(double observation, double new_value)
{
    Estimator::addObservation(observation);
    lastValue = new_value;
}

double ExternalEstimator::getEstimate()
{
    return lastValue;
}

void ExternalEstimator::storeNewObservation(double observation)
{
    /* ignore; the new estimator value is provided in 
     *  the overloaded addObservation above */
}
