#ifndef STRATEGY_EVALUATION_CONTEXT_H_INCL
#define STRATEGY_EVALUATION_CONTEXT_H_INCL

class Estimator;

class StrategyEvaluationContext {
  public:
    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;
};

#endif
