#ifndef SIMPLE_STATS_EXPECTATION_EVALUATOR
#define SIMPLE_STATS_EXPECTATION_EVALUATOR

#include "strategy_evaluator.h"

class SimpleStatsExpectationEvaluator : public StrategyEvaluator {
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
};

#endif
