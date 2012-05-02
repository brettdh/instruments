#include "simple_stats_expectation_evaluator.h"

EstimatorSet::SimpleStatsExpectationEvaluator::SimpleStatsExpectationEvaluator(EstimatorSet *owner_)
    : EstimatorSet::ExpectationEvaluator(owner_)
{
}

void 
EstimatorSet::SimpleStatsExpectationEvaluator::observationAdded(Estimator *estimator, double value)
{
    // TODO
}
