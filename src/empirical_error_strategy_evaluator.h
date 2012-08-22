#ifndef EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL
#define EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL

#include "strategy.h"
#include "strategy_evaluator.h"

class Estimator;
class JointDistribution;

class EmpiricalErrorStrategyEvaluator : public StrategyEvaluator {
  public:
    EmpiricalErrorStrategyEvaluator();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);
    
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
  private:
    JointDistribution *jointDistribution;
};

#endif
