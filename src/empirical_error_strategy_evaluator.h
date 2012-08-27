#ifndef EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL
#define EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL

#include "instruments.h"
#include "strategy.h"
#include "strategy_evaluator.h"
#include "eval_method.h"

class Estimator;
class AbstractJointDistribution;

class EmpiricalErrorStrategyEvaluator : public StrategyEvaluator {
  public:
    EmpiricalErrorStrategyEvaluator(EvalMethod eval_method);

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);
    
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
    virtual void setStrategies(const instruments_strategy_t *strategies_,
                               size_t num_strategies_);
  private:
    EmpiricalErrorEvalMethod eval_method;
    JointDistributionType joint_distribution_type;
    AbstractJointDistribution *jointDistribution;

    AbstractJointDistribution *createJointDistribution();
};

#endif
