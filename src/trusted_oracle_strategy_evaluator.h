#ifndef TRUSTED_ORACLE_STRATEGY_EVALUATOR_H_INCL
#define TRUSTED_ORACLE_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

class TrustedOracleStrategyEvaluator : public StrategyEvaluator {
  public:
    virtual void startIteration();
    virtual void finishIteration();
    virtual double jointProbability();
    virtual void advance();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
};

#endif
