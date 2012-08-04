#ifndef STRATEGY_EVALUATION_CONTEXT_H_INCL
#define STRATEGY_EVALUATION_CONTEXT_H_INCL

#include "instruments.h"
#include "small_map.h"

class Estimator;
struct EstimatorContext;

class StrategyEvaluationContext {
  public:
    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;

    virtual instruments_estimator_t getEstimatorContext(Estimator *estimator);
    virtual ~StrategyEvaluationContext();
  private:
    typedef small_map<Estimator *, EstimatorContext *> EstimatorMap;
    EstimatorMap estimators;
};

#endif
