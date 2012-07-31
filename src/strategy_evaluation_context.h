#ifndef STRATEGY_EVALUATION_CONTEXT_H_INCL
#define STRATEGY_EVALUATION_CONTEXT_H_INCL

#include <map>
#include "instruments.h"

class Estimator;
struct EstimatorContext;

class StrategyEvaluationContext {
  public:
    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;

    virtual instruments_estimator_t getEstimatorContext(Estimator *estimator);
    virtual ~StrategyEvaluationContext();
  private:
    std::map<Estimator *, EstimatorContext *> estimators;
};

#endif
