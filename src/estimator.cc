#include <stdlib.h>
#include <assert.h>
#include "estimator.h"
#include "estimator_set.h"
#include "running_mean_estimator.h"

Estimator *
Estimator::create()
{
    return create(DEFAULT_TYPE);
}

Estimator *
Estimator::create(EstimatorType type)
{
    switch (type) {
    case RUNNING_MEAN:
        return new RunningMeanEstimator();
    default:
        // TODO: implement more types
        assert(0);
    }
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
