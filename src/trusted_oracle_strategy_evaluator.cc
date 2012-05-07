#include "trusted_oracle_strategy_evaluator.h"
#include "estimator.h"

void 
TrustedOracleStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    // ignore values, trust the estimator
}

void
TrustedOracleStrategyEvaluator::startIteration()
{
    done = false;
}

void
TrustedOracleStrategyEvaluator::finishIteration()
{
}

double 
TrustedOracleStrategyEvaluator::jointProbability()
{
    return 1.0;
}

void 
TrustedOracleStrategyEvaluator::advance()
{
    done = true;
}

double
TrustedOracleStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // this evaluator assumes that the estimators are perfectly accurate,
    //  so here we just return the value.
    return estimator->getEstimate();
}
