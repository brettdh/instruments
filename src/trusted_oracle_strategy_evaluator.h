#ifndef TRUSTED_ORACLE_STRATEGY_EVALUATOR_H_INCL
#define TRUSTED_ORACLE_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

class TrustedOracleStrategyEvaluator : public StrategyEvaluator {
  public:
    virtual double expectedValue(typesafe_eval_fn_t fn, void *arg);
    virtual double getAdjustedEstimatorValue(Estimator *estimator);
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
};

#endif