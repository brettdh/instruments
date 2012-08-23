#include <stdlib.h>
#include <assert.h>
#include <float.h>
#include "strategy.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"
#include "estimator.h"

#include <set>
#include "small_set.h"

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
    collectEstimators();
}

/* fake context that just collects all the estimators 
 *  that the given strategy uses. */
class EstimatorCollector : public StrategyEvaluationContext {
    Strategy *strategy;
  public:
    EstimatorCollector(Strategy *s) : strategy(s) {}
    virtual double getAdjustedEstimatorValue(Estimator *estimator) {
        strategy->addEstimator(estimator);
        return estimator->getEstimate();
    }
};

void
Strategy::collectEstimators()
{
    // run all the eval functions and add all their estimators
    //  to this strategy.
    EstimatorCollector collector(this);
    if (time_fn) {
        (void)time_fn(&collector, strategy_arg, default_chooser_arg);
    }
    if (energy_cost_fn) {
        (void)energy_cost_fn(&collector, strategy_arg, default_chooser_arg);
    }
    if (data_cost_fn) {
        (void)data_cost_fn(&collector, strategy_arg, default_chooser_arg);
    }
}

void
Strategy::addEstimator(Estimator *estimator)
{
    estimators.insert(estimator);
}

void
Strategy::getAllEstimators(StrategyEvaluator *evaluator)
{
    for (small_set<Estimator*>::const_iterator it = estimators.begin();
         it != estimators.end(); ++it) {
        Estimator *estimator = *it;
        evaluator->addEstimator(estimator);
    }
}

bool
Strategy::usesEstimator(Estimator *estimator)
{
    return (estimators.count(estimator) != 0);
}

double
Strategy::calculateTime(StrategyEvaluator *evaluator, void *chooser_arg)
{
    return evaluator->expectedValue(this, time_fn, strategy_arg, chooser_arg);
}

double
Strategy::calculateCost(StrategyEvaluator *evaluator, void *chooser_arg)
{
    // TODO: finish implementing.  e.g. energy, goal-directed adaptation
    return evaluator->expectedValue(this, data_cost_fn, strategy_arg, chooser_arg);
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
}

const std::vector<Strategy *>&
Strategy::getChildStrategies()
{
    return child_strategies;
}

bool
Strategy::childrenAreDisjoint()
{
    std::set<Estimator *> all_estimators;
    for (size_t i = 0; i < child_strategies.size(); ++i) {
        Strategy *child = child_strategies[i];
        for (small_set<Estimator*>::const_iterator it = child->estimators.begin();
             it != child->estimators.end(); ++it) {
            if (all_estimators.count(*it) > 0) {
                return false;
            }
            all_estimators.insert(*it);
        }
    }
    return true;
}

typesafe_eval_fn_t 
Strategy::getEvalFn(eval_fn_type_t type)
{
    switch (type) {
    case TIME_FN:
        return time_fn;
    case ENERGY_FN:
        return energy_cost_fn;
    case DATA_FN:
        return data_cost_fn;
    }
    abort();
}
