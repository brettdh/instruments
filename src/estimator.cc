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
        estimator = new LastObservationEstimator(name);
        break;
    case RUNNING_MEAN:
        estimator = new RunningMeanEstimator(name);
        break;
    default:
        // TODO: implement more types
        abort();
    }
    return estimator;
}

Estimator::Estimator(const string& name_)
    : name(name_), has_estimate(false), has_range_hints(false)
{
    if (name.empty()) {
        throw runtime_error("Estimator name must not be empty");
    }
    
    for (size_t i = 0; i < name.length(); ++i) {
        if (isspace(name[i])) {
            name[i] = '_';
        }
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

bool 
Estimator::hasRangeHints()
{
    return has_range_hints;
}

EstimatorRangeHints 
Estimator::getRangeHints()
{
    assert(hasRangeHints());
    return range_hints;
}

void 
Estimator::setRangeHints(double min, double max, size_t num_bins)
{
    range_hints.min = min;
    range_hints.max = max;
    range_hints.num_bins = num_bins;
    has_range_hints = true;
}
