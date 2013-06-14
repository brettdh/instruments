#include <stdlib.h>
#include <assert.h>
#include <float.h>
#include "strategy.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"
#include "estimator.h"
#include "resource_weights.h"

#include <algorithm>
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
using std::find; using std::string; using std::ostringstream;
using std::hex;

#include "small_set.h"

#include "debug.h"
namespace inst = instruments;
using inst::INFO; using inst::ERROR;

Strategy::Strategy(eval_fn_t time_fn_, 
                   eval_fn_t energy_cost_fn_, 
                   eval_fn_t data_cost_fn_, 
                   void *strategy_arg_, 
                   void *default_chooser_arg_)
    : time_fn((typesafe_eval_fn_t) time_fn_),
      energy_cost_fn((typesafe_eval_fn_t) energy_cost_fn_),
      data_cost_fn((typesafe_eval_fn_t) data_cost_fn_),
      strategy_arg(strategy_arg_),
      default_chooser_arg(default_chooser_arg_)
{
    ASSERT(time_fn != energy_cost_fn);
    ASSERT(time_fn != data_cost_fn);
    ASSERT(energy_cost_fn != data_cost_fn);
    
    collectEstimators();
    setEvalFnLookupArray();

    ostringstream s;
    s << hex << this;
    name = s.str();
}

void
Strategy::setName(const char *name_)
{
    name = name_;
}

const char *
Strategy::getName() const
{
    return name.c_str();
}

void
Strategy::setEvalFnLookupArray()
{
    fns[TIME_FN] = time_fn;
    fns[ENERGY_FN] = energy_cost_fn;
    fns[DATA_FN] = data_cost_fn;
}

/* fake context that just collects all the estimators 
 *  that the given strategy uses. */
class EstimatorCollector : public StrategyEvaluationContext {
    Strategy *strategy;
    typesafe_eval_fn_t cur_fn;
  public:
    EstimatorCollector(Strategy *s) : strategy(s), cur_fn(NULL) {}
    void collect(typesafe_eval_fn_t fn, void *strategy_arg, void *chooser_arg) {
        if (fn) {
            cur_fn = fn;
            (void) fn(this, strategy_arg, chooser_arg);
        }
    }
    virtual double getAdjustedEstimatorValue(Estimator *estimator) {
        strategy->addEstimator(cur_fn, estimator);
        return estimator->getEstimate();
    }
};

void
Strategy::collectEstimators()
{
    // run all the eval functions and add all their estimators
    //  to this strategy.
    EstimatorCollector collector(this);
    collector.collect(time_fn, strategy_arg, default_chooser_arg);
    collector.collect(energy_cost_fn, strategy_arg, default_chooser_arg);
    collector.collect(data_cost_fn, strategy_arg, default_chooser_arg);
}

void
Strategy::addEstimator(typesafe_eval_fn_t fn, Estimator *estimator)
{
    estimators[fn].insert(estimator);
}

void
Strategy::getAllEstimators(StrategyEvaluator *evaluator)
{
    for (auto& pair : estimators) {
        auto& fn_estimators = pair.second;
        for (Estimator *estimator : fn_estimators) {
            evaluator->addEstimator(estimator);
        }
    }
}

bool
Strategy::usesEstimator(Estimator *estimator)
{
    for (auto& pair : estimators) {
        auto& fn_estimators = pair.second;
        if (fn_estimators.count(estimator) > 0) {
            return true;
        }
    }
    return false;
}

bool
Strategy::usesEstimator(typesafe_eval_fn_t fn, Estimator *estimator)
{
    return (estimators.count(fn) != 0 && estimators[fn].count(estimator) != 0);
}

bool
Strategy::usesNoEstimators(typesafe_eval_fn_t fn)
{
    return (estimators.count(fn) == 0 || estimators[fn].empty());
}

class AssertUnusedEvaluator : public StrategyEvaluationContext {
  public:
    virtual double getAdjustedEstimatorValue(Estimator *estimator) {
        inst::dbgprintf(ERROR, "should not be here; function uses no estimators\n");
        ASSERT(false);
        return 0.0;
    }
};

double 
Strategy::expectedValue(StrategyEvaluator *evaluator, typesafe_eval_fn_t fn, void *chooser_arg)
{
    if (fn == NULL) {
        return 0.0;
    }
    
    if (usesNoEstimators(fn)) {
        inst::dbgprintf(INFO, "No estimators; just calling eval fn\n");
        AssertUnusedEvaluator unused_evaluator;
        return fn(&unused_evaluator, strategy_arg, chooser_arg);
    } else {
        return evaluator->expectedValue(this, fn, strategy_arg, chooser_arg);
    }
}

double
Strategy::calculateTime(StrategyEvaluator *evaluator, void *chooser_arg)
{
    return expectedValue(evaluator, time_fn, chooser_arg);
}

double
Strategy::calculateCost(StrategyEvaluator *evaluator, void *chooser_arg)
{
    double energy_cost = expectedValue(evaluator, energy_cost_fn, chooser_arg);
    double data_cost = expectedValue(evaluator, data_cost_fn, chooser_arg);

    double energy_weight = get_energy_cost_weight();
    double data_weight = get_data_cost_weight();
    if (!evaluator->isSilent()) {
        inst::dbgprintf(INFO, "  Energy cost: %f * %f = %f\n",
                        energy_cost, energy_weight, energy_cost * energy_weight);
        inst::dbgprintf(INFO, "  Data cost:   %f * %f = %f\n",
                        data_cost, data_weight, data_cost * data_weight);
    }
    return ((energy_cost * energy_weight) + (data_cost * data_weight));
}

bool
Strategy::isRedundant()
{
    return !child_strategies.empty();
}

double
redundant_strategy_minimum_time(StrategyEvaluationContext *ctx, void *arg, void *chooser_arg)
{
    Strategy *parent = (Strategy *) arg;
    double best_time = DBL_MAX;
    for (size_t i = 0; i < parent->child_strategies.size(); ++i) {
        Strategy *child = parent->child_strategies[i];
        double time = child->time_fn(ctx, child->strategy_arg, chooser_arg);
        if (time < best_time) {
            best_time = time;
        }
    }
    return best_time;
}

double
redundant_strategy_total_energy_cost(StrategyEvaluationContext *ctx, void *arg, void *chooser_arg)
{
    Strategy *parent = (Strategy *) arg;
    double total_cost = 0.0;
    for (size_t i = 0; i < parent->child_strategies.size(); ++i) {
        Strategy *child = parent->child_strategies[i];
        if (child->energy_cost_fn) {
            double cost = child->energy_cost_fn(ctx, child->strategy_arg, chooser_arg);
            total_cost += cost;
        }
    }
    return total_cost;
}

double
redundant_strategy_total_data_cost(StrategyEvaluationContext *ctx, void *arg, void *chooser_arg)
{
    Strategy *parent = (Strategy *) arg;
    double total_cost = 0.0;
    for (size_t i = 0; i < parent->child_strategies.size(); ++i) {
        Strategy *child = parent->child_strategies[i];
        if (child->data_cost_fn) {
            double cost = child->data_cost_fn(ctx, child->strategy_arg, chooser_arg);
            total_cost += cost;
        }
    }
    return total_cost;
}

Strategy::Strategy(const instruments_strategy_t strategies[], 
                   size_t num_strategies)
    : time_fn(redundant_strategy_minimum_time),
      energy_cost_fn(redundant_strategy_total_energy_cost),
      data_cost_fn(redundant_strategy_total_data_cost),
      strategy_arg(this), default_chooser_arg(NULL)
{
    for (size_t i = 0; i < num_strategies; ++i) {
        this->child_strategies.push_back((Strategy *) strategies[i]);
    }
    collectEstimators();
    setEvalFnLookupArray();
}

const std::vector<Strategy *>&
Strategy::getChildStrategies()
{
    return child_strategies;
}

bool 
Strategy::includes(Strategy *child)
{
    // yes, it's O(n), but n is tiny.
    auto begin = child_strategies.begin();
    auto end = child_strategies.end();
    return find(begin, end, child) != end;
}

bool
Strategy::childrenAreDisjoint(typesafe_eval_fn_t fn)
{
    std::set<Estimator *> all_estimators;
    for (size_t i = 0; i < child_strategies.size(); ++i) {
        Strategy *child = child_strategies[i];
        for (Estimator *estimator :  child->estimators[fn]) {
            if (all_estimators.count(estimator) > 0) {
                return false;
            }
            all_estimators.insert(estimator);
        }
    }
    return true;
}

typesafe_eval_fn_t
Strategy::getEvalFn(eval_fn_type_t type)
{
    return fns[type];
}

std::set<Estimator *>
Strategy::getEstimatorsSet()
{
    // TODO: take fn argument?

    std::set<Estimator*> all_estimators;
    for (auto& pair : estimators) {
        auto& fn_estimators = pair.second;
        all_estimators.insert(fn_estimators.begin(), fn_estimators.end());
    }
    return all_estimators;
}

std::vector<Estimator *>
Strategy::getEstimators()
{
    std::set<Estimator *> all_estimators = getEstimatorsSet();
    return std::vector<Estimator *>(all_estimators.begin(), all_estimators.end());
}

static std::string value_names[] = {
    "time", "energy", "data"
};

std::string get_value_name(Strategy *strategy, typesafe_eval_fn_t fn)
{
    return value_names[get_value_type(strategy, fn)];
}

eval_fn_type_t
get_value_type(Strategy *strategy, typesafe_eval_fn_t fn)
{
    if (fn == strategy->getEvalFn(TIME_FN)) {
        return TIME_FN;
    } else if (fn == strategy->getEvalFn(ENERGY_FN)) {
        return ENERGY_FN;
    } else if (fn == strategy->getEvalFn(DATA_FN)) {
        return DATA_FN;
    } else abort();
}
