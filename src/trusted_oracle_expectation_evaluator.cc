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
