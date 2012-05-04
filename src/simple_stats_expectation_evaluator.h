#ifndef SIMPLE_STATS_EXPECTATION_EVALUATOR
#define SIMPLE_STATS_EXPECTATION_EVALUATOR

#include "estimator_set.h"
#include "expectation_evaluator.h"

class EstimatorSet::SimpleStatsExpectationEvaluator : public EstimatorSet::ExpectationEvaluator {
  protected:
    SimpleStatsExpectationEvaluator(EstimatorSet *owner_);
    virtual void observationAdded(Estimator *estimator, double value);
};

#endif
