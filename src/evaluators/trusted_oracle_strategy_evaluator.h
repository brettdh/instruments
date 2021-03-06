#ifndef TRUSTED_ORACLE_STRATEGY_EVALUATOR_H_INCL
#define TRUSTED_ORACLE_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

class TrustedOracleStrategyEvaluator : public StrategyEvaluator {
  public:
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg,
                                 ComparisonType comparison_type);
    virtual double getAdjustedEstimatorValue(Estimator *estimator);

    // nothing to save/restore.
    virtual void saveToFile(const char *filename) {}
    virtual void restoreFromFileImpl(const char *filename) {}
};

#endif
