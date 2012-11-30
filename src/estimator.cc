#include <stdlib.h>
#include <assert.h>
#include "estimator.h"
#include "last_observation_estimator.h"
#include "running_mean_estimator.h"
#include "strategy_evaluator.h"

#include <stdexcept>
#include <string>
using std::runtime_error; using std::string;

Estimator *
Estimator::create()
{
    return create(DEFAULT_TYPE);
}

Estimator *
Estimator::create(EstimatorType type)
{
    switch (type) {
    case LAST_OBSERVATION:
        return new LastObservationEstimator();
    case RUNNING_MEAN:
        return new RunningMeanEstimator();
    default:
        // TODO: implement more types
        abort();
    }
}

void
Estimator::addObservation(double value)
{
    for (small_set<StrategyEvaluator*>::const_iterator it = subscribers.begin();
         it != subscribers.end(); ++it) {
        StrategyEvaluator *subscriber = *it;
        subscriber->observationAdded(this, value);
    }
    storeNewObservation(value);
}

void
Estimator::subscribe(StrategyEvaluator *subscriber)
{
    subscribers.insert(subscriber);
}

string
Estimator::getName()
{
    // TODO: set name to enable error-checking when restoring distribution from file
    throw runtime_error("NOT IMPLEMENTED YET");
}
