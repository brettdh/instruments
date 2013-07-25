#include <stdlib.h>
#include <assert.h>
#include <float.h>
#include "estimator.h"
#include "last_observation_estimator.h"
#include "running_mean_estimator.h"
#include "strategy_evaluator.h"
#include "debug.h"
namespace inst = instruments;

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

Estimator::~Estimator()
{
    for (StrategyEvaluator *subscriber : subscribers) {
        subscriber->removeEstimator(this);
    }
}

bool estimate_is_valid(double estimate)
{
    return estimate != DBL_MAX;
}

double invalid_estimate()
{
    double e = DBL_MAX;
    ASSERT(!estimate_is_valid(e));
    return e;
}

void
Estimator::addObservation(double observation)
{
    double old_estimate = invalid_estimate(), new_estimate = invalid_estimate();
    if (has_estimate) {
        old_estimate = getEstimate();
    }
    storeNewObservation(observation);
    has_estimate = true;
    new_estimate = getEstimate();

    for (StrategyEvaluator *subscriber : subscribers) {
        subscriber->observationAdded(this, observation, old_estimate, new_estimate);
    }
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

void
Estimator::unsubscribe(StrategyEvaluator *unsubscriber)
{
    subscribers.erase(unsubscriber);
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
    ASSERT(hasRangeHints());
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

void
Estimator::setCondition(enum ConditionType type, double value)
{
    conditions[type] = value;
    ConditionType other_type = (type == AT_MOST ? AT_LEAST : AT_MOST);
    if (conditions.count(other_type) > 0) {
        if (conditions[AT_LEAST] > conditions[AT_MOST]) {
            inst::dbgprintf(inst::ERROR, "Warning: tried to set impossible conditions [>= %f, <= %f] on estimator %s (both are now %f)\n",
                            conditions[AT_LEAST], conditions[AT_MOST], name.c_str(), conditions[other_type]);
            conditions[type] = conditions[other_type];
        }
    }

    for (StrategyEvaluator *subscriber : subscribers) {
        subscriber->estimatorConditionsChanged(this);
    }
}

void Estimator::clearConditions()
{
    conditions.clear();
    for (StrategyEvaluator *subscriber : subscribers) {
        subscriber->estimatorConditionsChanged(this);
    }
}

bool
Estimator::valueMeetsConditions(double value)
{
    return ((conditions.count(AT_LEAST) == 0 || value >= conditions[AT_LEAST]) &&
            (conditions.count(AT_MOST) == 0 || value <= conditions[AT_MOST]));
}

double 
Estimator::getConditionalWeight(double value)
{
    /*
     * The weighting is bogus.  What's really going on here is just
     * a conditional probability expectation, which  means that
     * the weights on out-of-bounds values should just be zero.
     
    if (conditions.count(AT_LEAST) > 0 && value < conditions[AT_LEAST]) {
        // further below bound = lower weight
        return value / conditions[AT_LEAST];
    } else if (conditions.count(AT_MOST) > 0 && value > conditions[AT_MOST]) {
        // further above bound = lower weight
        return conditions[AT_MOST] / value;
    */
    if ((conditions.count(AT_LEAST) > 0 && value < conditions[AT_LEAST]) ||
        (conditions.count(AT_MOST) > 0 && value > conditions[AT_MOST])) {
        return 0.0;
    } else {
        // within bounds = even weight
        return 1.0;
    }
}
