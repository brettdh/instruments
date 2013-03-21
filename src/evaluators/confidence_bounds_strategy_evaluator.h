#ifndef CONFIDENCE_BOUNDS_STRATEGY_EVALUATOR_H_INCL
#define CONFIDENCE_BOUNDS_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

class ConfidenceBoundsStrategyEvaluator : public StrategyEvaluator {
  public:
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);
    virtual double getAdjustedEstimatorValue(Estimator *estimator);

    // nothing to save/restore.
    virtual void saveToFile(const char *filename) {}
    virtual void restoreFromFile(const char *filename) {}
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
};

#endif
