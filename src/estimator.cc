#include <stdlib.h>
#include <assert.h>
#include <float.h>
#include <math.h>
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

    inst::dbgprintf(inst::INFO, "Setting %s bound for estimator %s: %f\n", 
                    type == AT_MOST ? "upper" : "lower", name.c_str(), value);

    for (StrategyEvaluator *subscriber : subscribers) {
        subscriber->estimatorConditionsChanged(this);
    }
}

void Estimator::clearConditions()
{
    inst::dbgprintf(inst::INFO, "Clearing bounds for estimator %s\n",
                    name.c_str());
    conditions.clear();
    for (StrategyEvaluator *subscriber : subscribers) {
        subscriber->estimatorConditionsChanged(this);
    }
}


#define THRESHOLD 0.0001

#define float_equal_or_cmp(a, cmp, b) \
    ( (fabs((a) - (b)) < THRESHOLD) || ((a) cmp (b)) )

inline double
float_is_greater_or_equal(double a, double b)
{
    return float_equal_or_cmp(a, >, b);
}

inline double
float_is_less_or_equal(double a, double b)
{
    return float_equal_or_cmp(a, <, b);
}

bool
Estimator::valueMeetsConditions(double value)
{
    return ((conditions.count(AT_LEAST) == 0 || float_is_greater_or_equal(value, conditions[AT_LEAST])) &&
            (conditions.count(AT_MOST) == 0 || float_is_less_or_equal(value, conditions[AT_MOST])));
}

bool
Estimator::hasConditions()
{
    return ((conditions.count(AT_LEAST) +
             conditions.count(AT_MOST)) > 0);
}

double
Estimator::getLowerBound()
{
    return conditions.count(AT_LEAST) == 0 ? DBL_MIN : conditions[AT_LEAST];
}

double
Estimator::getUpperBound()
{
    return conditions.count(AT_MOST) == 0 ? DBL_MAX : conditions[AT_MOST];
}

double 
Estimator::getConditionalBound()
{
    ASSERT(hasConditions());
    
    if (conditions.count(AT_LEAST) > 0 && conditions.count(AT_MOST) > 0) {
        return (conditions[AT_LEAST] + conditions[AT_MOST]) / 2.0;
    } else if (conditions.count(AT_LEAST) > 0) {
        return conditions[AT_LEAST];
    } else if (conditions.count(AT_MOST) > 0) {
        return conditions[AT_MOST];
    } else {
        ASSERT(false);
        __builtin_unreachable();
    }
}
