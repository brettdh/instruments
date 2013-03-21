#include "confidence_bounds_strategy_evaluator.h"
#include "estimator.h"

void 
ConfidenceBoundsStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    // ignore values, trust the estimator
}

double
ConfidenceBoundsStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                              void *strategy_arg, void *chooser_arg)
{
    // no weighted sum; just trust the estimators and evaluate the function
    return fn(this, strategy_arg, chooser_arg);
}

double
ConfidenceBoundsStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // this evaluator assumes that the estimators are perfectly accurate,
    //  so here we just return the value.
    return estimator->getEstimate();
}
