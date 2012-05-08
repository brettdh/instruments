#include "trusted_oracle_strategy_evaluator.h"
#include "estimator.h"

void 
TrustedOracleStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    // ignore values, trust the estimator
}

double
TrustedOracleStrategyEvaluator::expectedValue(typesafe_eval_fn_t fn, void *arg)
{
    // no weighted sum; just trust the estimators and evaluate the function
    return fn(this, arg);
}

double
TrustedOracleStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // this evaluator assumes that the estimators are perfectly accurate,
    //  so here we just return the value.
    return estimator->getEstimate();
}
