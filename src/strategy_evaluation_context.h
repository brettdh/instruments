#ifndef STRATEGY_EVALUATION_CONTEXT_H_INCL
#define STRATEGY_EVALUATION_CONTEXT_H_INCL

#include "instruments.h"

class Estimator;
struct EstimatorContext;

class StrategyEvaluationContext {
  public:
    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;
};

#endif
