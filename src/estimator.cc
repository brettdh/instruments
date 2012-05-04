#include <stdlib.h>
#include "estimator.h"
#include "estimator_set.h"
#include "running_mean_estimator.h"

Estimator *
Estimator::create()
{
    // for now: default type.
    // TODO: choose between types.
    return new RunningMeanEstimator();
}

Estimator::Estimator() 
    : owner(NULL)
{
}

void
Estimator::addObservation(double value)
{
    if (owner) {
        owner->observationAdded(this, value);
    }
    storeNewObservation(value);
}

void
Estimator::setOwner(EstimatorSet *owner_)
{
    owner = owner_;
}
