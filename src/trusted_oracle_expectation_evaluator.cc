#include "trusted_oracle_expectation_evaluator.h"

EstimatorSet::TrustedOracleExpectationEvaluator::TrustedOracleExpectationEvaluator(EstimatorSet *owner_)
    : EstimatorSet::ExpectationEvaluator(owner_)
{
}

void 
EstimatorSet::TrustedOracleExpectationEvaluator::observationAdded(Estimator *estimator, double value)
{
    // ignore values, trust the estimator
}

void
EstimatorSet::TrustedOracleExpectationEvaluator::startIterator()
{
    done = false;
}

void
EstimatorSet::TrustedOracleExpectationEvaluator::finishIteration()
{
}

double 
EstimatorSet::TrustedOracleExpectationEvaluator::jointProbability()
{
    return 1.0;
}

void 
EstimatorSet::TrustedOracleExpectationEvaluator::advance()
{
    done = true;
}

double
EstimatorSet::TrustedOracleExpectationEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // this evaluator assumes that the estimators are perfectly accurate,
    //  so here we just return the value.
    return estimator->getEstimate();
}
