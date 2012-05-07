#include "strategy.h"
#include "strategy_evaluator.h"

Strategy::Strategy(eval_fn_t time_fn_, 
                   eval_fn_t energy_cost_fn_, 
                   eval_fn_t data_cost_fn_, 
                   void *fn_arg_)
    : time_fn(time_fn_),
      energy_cost_fn(energy_cost_fn_),
      data_cost_fn(data_cost_fn_),
      fn_arg(fn_arg_)
{
}

void
Strategy::addEstimator(Estimator *estimator)
{
    estimators.insert(estimator);
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
