#include <float.h>
#include "strategy.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"
#include "estimator.h"

#include <set>
using std::set;

Strategy::Strategy(eval_fn_t time_fn_, 
                   eval_fn_t energy_cost_fn_, 
                   eval_fn_t data_cost_fn_, 
                   void *fn_arg_)
    : time_fn((typesafe_eval_fn_t) time_fn_),
      energy_cost_fn((typesafe_eval_fn_t) energy_cost_fn_),
      data_cost_fn((typesafe_eval_fn_t) data_cost_fn_),
      fn_arg(fn_arg_)
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
        return estimator->getEstimate();
    }
    
    virtual instruments_estimator_t getEstimatorContext(Estimator *estimator) {
        strategy->addEstimator(estimator);
        return this->StrategyEvaluationContext::getEstimatorContext(estimator);
    }
};

void
Strategy::collectEstimators()
{
    // run all the eval functions and add all their estimators
    //  to this strategy.
    EstimatorCollector collector(this);
    if (time_fn) {
        (void)time_fn(&collector, fn_arg);
    }
    if (energy_cost_fn) {
        (void)energy_cost_fn(&collector, fn_arg);
    }
    if (data_cost_fn) {
        (void)data_cost_fn(&collector, fn_arg);
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
    for (set<Estimator*>::const_iterator it = estimators.begin();
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
Strategy::calculateTime(StrategyEvaluator *evaluator)
{
    return evaluator->expectedValue(time_fn, fn_arg);
}

double
Strategy::calculateCost(StrategyEvaluator *evaluator)
{
    // TODO: finish implementing.  e.g. energy, goal-directed adaptation
    return evaluator->expectedValue(data_cost_fn, fn_arg);
}

bool
Strategy::isRedundant()
{
    return !child_strategies.empty();
}

double
redundant_strategy_minimum_time(StrategyEvaluationContext *ctx, void *arg)
{
    Strategy *parent = (Strategy *) arg;
    double best_time = DBL_MAX;
    for (size_t i = 0; i < parent->child_strategies.size(); ++i) {
        Strategy *child = parent->child_strategies[i];
        double time = child->time_fn(ctx, child->fn_arg);
        if (time < best_time) {
            best_time = time;
        }
    }
    return best_time;
}

double
redundant_strategy_total_energy_cost(StrategyEvaluationContext *ctx, void *arg)
{
    Strategy *parent = (Strategy *) arg;
    double total_cost = 0.0;
    for (size_t i = 0; i < parent->child_strategies.size(); ++i) {
        Strategy *child = parent->child_strategies[i];
        double cost = child->energy_cost_fn(ctx, child->fn_arg);
        total_cost += cost;
    }
    return total_cost;
}

double
redundant_strategy_total_data_cost(StrategyEvaluationContext *ctx, void *arg)
{
    Strategy *parent = (Strategy *) arg;
    double total_cost = 0.0;
    for (size_t i = 0; i < parent->child_strategies.size(); ++i) {
        Strategy *child = parent->child_strategies[i];
        double cost = child->data_cost_fn(ctx, child->fn_arg);
        total_cost += cost;
    }
    return total_cost;
}

Strategy::Strategy(const instruments_strategy_t strategies[], 
                   size_t num_strategies)
    : time_fn(redundant_strategy_minimum_time),
      energy_cost_fn(redundant_strategy_total_energy_cost),
      data_cost_fn(redundant_strategy_total_data_cost),
      fn_arg(this)
{
    for (size_t i = 0; i < num_strategies; ++i) {
        this->child_strategies.push_back((Strategy *) strategies[i]);
    }
}
