#include "external_estimator.h"

ExternalEstimator::ExternalEstimator(const std::string& name)
    : Estimator(name), lastValue(0.0), stagedValue(0.0)
{
}

void ExternalEstimator::addObservation(double observation, double new_value)
{
    // stage it, so I can update at the right moment
    stagedValue = new_value;
    
    Estimator::addObservation(observation);
}

double ExternalEstimator::getEstimate()
{
    return lastValue;
}

void ExternalEstimator::storeNewObservation(double observation)
{
    /* the new estimator value is provided in 
     *  the overloaded addObservation above */
    lastValue = stagedValue;
}
