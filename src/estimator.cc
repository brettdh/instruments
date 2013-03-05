#include <stdlib.h>
#include <assert.h>
#include "estimator.h"
#include "last_observation_estimator.h"
#include "running_mean_estimator.h"
#include "strategy_evaluator.h"

#include <stdexcept>
#include <string>
#include <sstream>
using std::runtime_error; using std::string;
using std::ostringstream;

Estimator *
Estimator::create(string name)
{
    return create(DEFAULT_TYPE, name);
}

Estimator *
Estimator::create(EstimatorType type, string name)
{
    Estimator *estimator = NULL;
    switch (type) {
    case LAST_OBSERVATION:
        estimator = new LastObservationEstimator();
        break;
    case RUNNING_MEAN:
        estimator = new RunningMeanEstimator();
        break;
    default:
        // TODO: implement more types
        abort();
    }
    if (name.empty()) {
        name = nextDefaultName();
    }
    estimator->name = name;
    return estimator;
}

int Estimator::numNamelessEstimators = 0;

string
Estimator::nextDefaultName()
{
    ostringstream s;
    s << "Estimator-" << ++numNamelessEstimators;
    return s.str();
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
    has_estimate = true;
}

bool
Estimator::hasEstimate()
{
    return has_estimate;
}

void
Estimator::subscribe(StrategyEvaluator *subscriber)
{
    subscribers.insert(subscriber);
}

string
Estimator::getName()
{
    return name;
}
