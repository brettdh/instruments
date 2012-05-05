#include "strategy.h"
#include "estimator_set.h"

Strategy::Strategy(eval_fn_t time_fn_, 
                   eval_fn_t energy_cost_fn_, 
                   eval_fn_t data_cost_fn_, 
                   void *fn_arg_)
    : time_fn(time_fn_),
      energy_cost_fn(energy_cost_fn_),
      data_cost_fn(data_cost_fn_),
      fn_arg(fn_arg_)
{
    // TODO: don't hardcode the evaluation mode; make it an argument?
    estimators = EstimatorSet::create(SIMPLE_STATS);
}

void
Strategy::addEstimator(Estimator *estimator)
{
    estimators->addEstimator(estimator);
}

double 
Strategy::calculateTime()
{
    return estimators->expectedValue(time_fn, fn_arg);
}

double
Strategy::calculateCost()
{
    // TODO: finish implementing.  e.g. energy, goal-directed adaptation
    return estimators->expectedValue(data_cost_fn, fn_arg);
}
