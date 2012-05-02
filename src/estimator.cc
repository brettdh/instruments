#include <stdlib.h>
#include "estimator.h"
#include "estimator_set.h"

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
